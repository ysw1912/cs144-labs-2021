#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

static_assert(sizeof(size_t) == sizeof(uint64_t));

class UnAssembleBuffer {
  private:
    std::vector<char> buffer_;  //!< Cycle buffer.
    std::map<size_t, size_t> index_map_{};

    size_t used_size_ = 0;      //!< Used size of buffer.
    size_t start_pos_ = 0;

    //! \brief When pushing a substring into the buffer, we record the interval
    //! corresponding to the string index into the map. We need to deal with
    //! interval merging due to the overlapping case.
    void MergeInterval(size_t index, size_t str_size);

  public:
    explicit UnAssembleBuffer(size_t capacity);

    //! \brief Push a substring into the buffer.
    //!
    //! \param data the substring view.
    //! \param index indicates the index (place in sequence) of the first byte in `data`.
    //! \param start_index indicates the index (place in sequence) of the start position.
    std::string push_substring(std::string_view data, size_t index, size_t start_index);

    bool empty() const;

    size_t used_size() const { return used_size_; }
};

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    UnAssembleBuffer buffer_;   //!< Buffer storing unassembled substrings.
    ByteStream output_;         //!< The reassembled in-order byte stream.
    size_t capacity_;
    std::optional<uint64_t> eof_index_{};

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    explicit StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return output_; }
    ByteStream &stream_out() { return output_; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
