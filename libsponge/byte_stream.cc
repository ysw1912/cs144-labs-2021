#include "byte_stream.hh"

#include <cassert>
#include <cstring>

#include <string>

ByteStream::ByteStream(const size_t capacity)
    : buffer_(capacity, 0) {}

size_t ByteStream::write(const std::string &data) {
    size_t write_size = std::min(data.size(), remaining_capacity());
    if (write_size == 0) {
        return 0;
    }
    size_t end = end_pos();
    if (end < start_pos_ || end + write_size <= buffer_.capacity()) {
        ::memcpy(buffer_.data() + end, data.data(), write_size);
    } else {
        size_t copy1 = buffer_.capacity() - end;
        ::memcpy(buffer_.data() + end, data.data(), copy1);
        ::memcpy(buffer_.data(), data.data() + copy1, write_size - copy1);
    }
    used_size_ += write_size;
    assert(used_size_ <= buffer_.capacity());
    bytes_written_ += write_size;
    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
std::string ByteStream::peek_output(const size_t len) const {
    size_t read_size = std::min(len, used_size_);
    std::string ret;
    ret.resize(read_size);
    if (start_pos_ + read_size <= buffer_.capacity()) {
        ::memcpy(ret.data(), buffer_.data() + start_pos_, read_size);
    } else {
        size_t copy1 = buffer_.capacity() - start_pos_;
        ::memcpy(ret.data(), buffer_.data() + start_pos_, copy1);
        ::memcpy(ret.data() + copy1, buffer_.data(), read_size - copy1);
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_size = std::min(len, used_size_);
    start_pos_ = (start_pos_ + pop_size) % buffer_.capacity();
    used_size_ -= pop_size;
    bytes_read_ += pop_size;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() {
    input_ended_ = true;
}

bool ByteStream::input_ended() const { return input_ended_; }

size_t ByteStream::buffer_size() const { return used_size_; }

bool ByteStream::buffer_empty() const { return used_size_ == 0; }

bool ByteStream::eof() const { return used_size_ == 0 && input_ended_; }

size_t ByteStream::bytes_written() const { return bytes_written_; }

size_t ByteStream::bytes_read() const { return bytes_read_; }

size_t ByteStream::remaining_capacity() const { return buffer_.capacity() - used_size_; }

size_t ByteStream::end_pos() const { return (start_pos_ + used_size_) % buffer_.capacity(); }
