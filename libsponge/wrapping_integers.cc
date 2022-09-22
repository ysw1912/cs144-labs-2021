#include "wrapping_integers.hh"

#include <cassert>

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + n; }

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
    WrappingInt32 cp = wrap(checkpoint, isn);
    // Positive diff and negative diff.
    uint32_t diff_p, diff_n;
    if (cp.raw_value() < n.raw_value()) {
        // 0 1 2 3 4 5 6 7
        //  cp=1      n=6
        diff_p = n - cp;
        diff_n = std::numeric_limits<uint32_t>::max() - n.raw_value() + cp.raw_value() + 1;
    } else {
        // 0 1 2 3 4 5 6 7
        //  n=1      cp=6
        diff_p = std::numeric_limits<uint32_t>::max() - cp.raw_value() + n.raw_value() + 1;
        diff_n = cp - n;
    }
    if (diff_p < diff_n || checkpoint < diff_n) {
        return checkpoint + diff_p;
    }
    return checkpoint - diff_n;
}
