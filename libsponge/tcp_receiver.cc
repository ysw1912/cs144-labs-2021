#include "tcp_receiver.hh"

#include <cassert>

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (state() == State::kListen) {
        if (!seg.header().syn) {
            // Skip until receiving SYN.
            return;
        }
        // Handle the SYN.
        is_listen_ = false;
        isn_ = seg.header().seqno;
    }
    // Handle the payload or FIN, both could be in the same segment with SYN.
    // SYN occupies one seq no, so need to plus one if SYN was set.
    uint64_t abs_seq_no = unwrap(seg.header().seqno + seg.header().syn, isn_.value(), abs_ack_no());
    // Only if |abs_seq_no| > 0, |stream_idx| (starting at 0) is legal.
    if (abs_seq_no > 0) {
        uint64_t stream_idx = abs_seq_no - 1;
        reassembler_.push_substring(seg.payload().copy(), stream_idx, seg.header().fin);
    }
}

uint64_t TCPReceiver::abs_ack_no() const {
    State s = state();
    if (s == State::kSynRecv) {
        return reassembler_.stream_out().bytes_written() + 1;
    } else if (s == State::kFinRecv) {
        return reassembler_.stream_out().bytes_written() + 2;
    }
    return 0;
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    State s = state();
    if (s == State::kError || s == State::kListen) {
        return std::nullopt;
    }
    return wrap(abs_ack_no(), isn_.value());
}

size_t TCPReceiver::window_size() const {
    return capacity_ - reassembler_.stream_out().buffer_size();
}

TCPReceiver::State TCPReceiver::state() const {
    if (reassembler_.stream_out().error()) {
        return State::kError;
    }
    if (is_listen_) {
        return State::kListen;
    }
    if (reassembler_.stream_out().input_ended()) {
        return State::kFinRecv;
    }
    return State::kSynRecv;
}
