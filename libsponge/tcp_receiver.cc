#include "tcp_receiver.hh"

#include <cassert>

void TCPReceiver::segment_received(const TCPSegment &seg) {
    WrappingInt32 payload_seq_no = seg.header().seqno;
    // Handle the first SYN.
    if (!syn_recv() && seg.header().syn) {
        isn_ = seg.header().seqno;
        ack_no_ = isn_.value() + 1;
        payload_seq_no = payload_seq_no + 1;
    }
    if (!syn_recv()) {
        return;
    }
    // Handle the payload, which may be in the same segment with SYN.
    if (seg.payload().size() > 0) {
        uint64_t abs_seq_no = unwrap(payload_seq_no, isn_.value(), checkpoint_);
        // Only if |abs_seq_no| > 0, |stream_idx| (starting at 0) is legal.
        if (abs_seq_no > 0) {
            uint64_t stream_idx = abs_seq_no - 1;
            reassembler_.push_substring(seg.payload().copy(), stream_idx, seg.header().fin);
            // Use the index of the last reassembled byte as the checkpoint.
            checkpoint_ = reassembler_.stream_out().bytes_written();
            uint64_t abs_ack_no = checkpoint_ + 1;
            ack_no_ = wrap(abs_ack_no, isn_.value());
        }
    }
    // Handle FIN, which may be in the same segment with SYN or payload.
    // It is possible that FIN has been received in the past, but cannot be assembled yet, so we need to
    // consider the case of |stream_out().input_ended()|.
    if (seg.header().fin || reassembler_.stream_out().input_ended()) {
        // Only when all the data is assembled, we consider FIN received and ACK is to be added 1.
        if (reassembler_.unassembled_bytes() == 0) {
            reassembler_.stream_out().end_input();
            ack_no_ = ack_no_.value() + 1;
        }
    }
}

std::optional<WrappingInt32> TCPReceiver::ackno() const { return ack_no_; }

size_t TCPReceiver::window_size() const {
    return capacity_ - reassembler_.stream_out().buffer_size();
}
