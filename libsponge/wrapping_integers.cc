#include "wrapping_integers.hh"

#include <cassert>

constexpr uint64_t k2ToPow32 = 1ull << 32;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t seq_no = (n % k2ToPow32 + static_cast<uint64_t>(isn.raw_value())) % k2ToPow32;
    return WrappingInt32{static_cast<uint32_t>(seq_no)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t offset_32;
    if (isn.raw_value() <= n.raw_value()) {
        offset_32 = n - isn;
    } else {
        offset_32 = std::numeric_limits<uint32_t>::max() - isn.raw_value() + 1 + n.raw_value();
    }
    auto offset = static_cast<uint64_t>(offset_32);
    if (checkpoint <= offset) {
        return offset;
    }
    //                           checkpoint
    //                               |
    // offset | k2ToPow32 | ... | k2ToPow32 |
    //                          |           |
    //                        abs_seq_1         abs_seq_2
    //        |<--=- count ---->|
    //
    uint64_t count = (checkpoint - offset) >> 32;
    uint64_t abs_seq_1 = offset + k2ToPow32 * count;
    uint64_t abs_seq_2 = offset + k2ToPow32 * (count + 1);
    // Choose the seq no closest to the checkpoint.
    return checkpoint - abs_seq_1 <= abs_seq_2 - checkpoint ? abs_seq_1 : abs_seq_2;
}
