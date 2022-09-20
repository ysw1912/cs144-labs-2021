#include "stream_reassembler.hh"

#include <cassert>
#include <cstring>

UnAssembleBuffer::UnAssembleBuffer(size_t capacity) : buffer_(capacity) {}

std::string UnAssembleBuffer::push_substring(std::string_view data, size_t index, size_t start_index) {
    // Caller need to ensure that ｜data｜ does not have the prefix already written to output.
    assert(index >= start_index);
    // Caller need to ensure that all the |data| can fit into the buffer.
    assert(index - start_index < buffer_.capacity());
    // Push substring to buffer first.
    size_t pos = (start_pos_ + index - start_index) % buffer_.capacity();
    if (pos + data.size() <= buffer_.capacity()) {
        ::memcpy(buffer_.data() + pos, data.data(), data.size());
    } else {
        size_t copy1 = buffer_.capacity() - pos;
        ::memcpy(buffer_.data() + pos, data.data(), copy1);
        ::memcpy(buffer_.data(), data.data() + copy1, data.size() - copy1);
    }

    MergeInterval(index, data.size());

    if (index_map_.empty() || index_map_.begin()->first != start_index) {
        return {};
    }
    size_t str_size = index_map_.begin()->second;
    used_size_ -= str_size;
    index_map_.erase(index_map_.begin());

    std::string popped(str_size, 0);
    if (start_pos_ + str_size <= buffer_.capacity()) {
        ::memcpy(popped.data(), buffer_.data() + start_pos_, str_size);
    } else {
        size_t copy1 = buffer_.capacity() - start_pos_;
        ::memcpy(popped.data(), buffer_.data() + start_pos_, copy1);
        ::memcpy(popped.data() + copy1, buffer_.data(), str_size - copy1);
    }
    start_pos_ = (start_pos_ + str_size) % buffer_.capacity();
    return popped;
}

void UnAssembleBuffer::MergeInterval(size_t index, size_t str_size) {
    if (index_map_.empty()) {
        index_map_[index] = str_size;
        used_size_ += str_size;
        return;
    }
    for (auto iter = index_map_.begin(); iter != index_map_.end();) {
        // [index, ...] ... [iter->first, ...]
        if (index + str_size < iter->first) {
            index_map_.emplace_hint(iter, index, str_size);
            used_size_ += str_size;
            return;
        }
        // [iter->first, ...] ... [index, ...]
        if (index > iter->first + iter->second) {
            iter++;
            continue;
        }
        // Interval overlapping.
        size_t last = std::max(index + str_size, iter->first + iter->second);
        index = std::min(index, iter->first);
        str_size = last - index;
        used_size_ -= iter->second;
        iter = index_map_.erase(iter);
    }
    index_map_.emplace_hint(index_map_.end(), index, str_size);
    used_size_ += str_size;
}

bool UnAssembleBuffer::empty() const {
    return used_size_ == 0;
}

StreamReassembler::StreamReassembler(const size_t capacity)
    : buffer_(capacity), output_(capacity), capacity_(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const std::string& data, uint64_t index, const bool eof) {
    std::string_view sv = data;
    size_t bytes_written = output_.bytes_written();

    // All the |data| has been written.
    if (index + sv.size() < bytes_written) {
        return;
    }
    // - - - x x x x x x x x
    //       |       |
    //     index  bytes_written
    //
    // If index = 3, bytes_written = 7, sv.size() = 8, we just need to push the last 4.
    if (index < bytes_written) {
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

    if (eof_index_ && output_.bytes_written() == eof_index_.value()) {
        output_.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return buffer_.used_size(); }

bool StreamReassembler::empty() const { return buffer_.empty(); }
