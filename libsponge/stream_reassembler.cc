#include "stream_reassembler.hh"

#include <cassert>

UnAssembleBuffer::UnAssembleBuffer(size_t capacity) : buffer_(capacity) {}

std::string UnAssembleBuffer::push_substring(std::string_view data, size_t index, size_t start_index) {
    std::string popped;
    assert(index >= start_index);
    // Caller need to ensure that all the |data| can fit into the buffer.
    assert(index - start_index < buffer_.capacity());
    // 1. Push substring to buffer first.
    size_t pos = (start_pos_ + index - start_index) % buffer_.capacity();
    size_t incremental = 0;  // The number of characters added in this push.
    for (size_t i = 0; i < data.size(); i++) {
        size_t curr = (pos + i) % buffer_.capacity();
        buffer_[curr].ch = data[i];
        if (!buffer_[curr].used) {
            buffer_[curr].used = true;
            incremental++;
        }
        if (i == data.size() - 1) {
            size_t last_pos = (curr + 1) % buffer_.capacity();
            if (empty() ||
                (start_pos_ < end_pos_ && (end_pos_ < last_pos || last_pos <= start_pos_)) ||
                (end_pos_ < last_pos && last_pos <= start_pos_)) {
                // Always update |end_pos_| when the buffer is empty.
                // When not empty, 3 cases where we need to update |end_pos_|:
                //   [ ... start_pos_ ... end_pos_ ... last_pos ... ]
                //   [ ... last_pos ... start_pos_ ... end_pos_ ... ]
                //   [ ... end_pos_ ... last_pos ... start_pos_ ... ]
                end_pos_ = last_pos;
            }
            // Update |used_size_| at the end so as not to affect the result of |empty()|.
            used_size_ += incremental;
        }
    }
    // 2. If the hole at the beginning has been filled, the beginning substring will be popped out.
    while (!empty() && buffer_[start_pos_].used) {
        popped.push_back(buffer_[start_pos_].ch);
        buffer_[start_pos_].used = false;
        used_size_--;
        start_pos_ = (start_pos_ + 1) % buffer_.capacity();
    }
    return popped;
}

bool UnAssembleBuffer::empty() const {
    return start_pos_ == end_pos_ && used_size_ == 0;
}

StreamReassembler::StreamReassembler(const size_t capacity)
    : buffer_(capacity), output_(capacity), capacity_(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const std::string& data, size_t index, const bool eof) {
    std::string_view sv = data;
    size_t bytes_written = output_.bytes_written();

    // - - - x x x x x x x x
    //       |       |
    //     index  bytes_written
    //
    // If index = 3, bytes_written = 7, sv.size() = 8, we just need to push the last 4.
    if (index < bytes_written) {
        if (bytes_written - index > sv.size()) {
            // All the |data| has been written.
            return;
        }
        sv.remove_prefix(bytes_written - index);
        index = bytes_written;
    }
    assert(index >= bytes_written);

    //          |<--capacity--->|
    // - - - - - 0 0 0 0 0 0 x x x x
    //           |           |
    //     bytes_written    index
    //
    // If capacity = 8, sv.size() = 4, we can only push the first 2.
    size_t max_len = capacity_ - (index - bytes_written);
    if (sv.size() > max_len) {
        sv = sv.substr(0, max_len);
    }

    // Set |eof_index_|.
    if (eof) {
        eof_index_ = index + sv.size();
    }

    if (!sv.empty()) {
        std::string popped = buffer_.push_substring(sv, index, bytes_written);
        if (!popped.empty()) {
            output_.write(popped);
        }
    }

    if (output_.bytes_written() == eof_index_) {
        output_.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return buffer_.used_size(); }

bool StreamReassembler::empty() const { return buffer_.empty(); }
