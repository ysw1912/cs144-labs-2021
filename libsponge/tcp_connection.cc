#include "tcp_connection.hh"

#include <iostream>

size_t TCPConnection::remaining_outbound_capacity() const {
    return sender_.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const { return sender_.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return receiver_.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return ms_since_last_recv_; }

void TCPConnection::enqueue_segments() {
    while (!sender_.segments_out().empty()) {
        TCPSegment seg = std::move(sender_.segments_out().front());
        if (need_send_rst_) {
            seg.header().rst = true;
            segments_out_.emplace(seg);
            return;
        }
        // Before sending, we will ask the receiver for the ack no and window size.
        if (receiver_.ackno()) {
            seg.header().ack = true;
            seg.header().ackno = receiver_.ackno().value();
            seg.header().win = static_cast<uint16_t>(std::min(
                receiver_.window_size(),
                static_cast<size_t>(std::numeric_limits<uint16_t>::max())
            ));
        }
        segments_out_.emplace(seg);
        sender_.segments_out().pop();
    }
}

void TCPConnection::try_clean_shutdown() {
    // The connection is closed when:
    // case 1: Active close, FIN_RECV, the lingering timer expired.
    // case 2: Passive close, FIN_ACKED.
    if ((linger_after_stream_finish_ &&
         receiver_.state() == TCPReceiver::State::kFinRecv &&
         ms_since_last_recv_ >= 10 * cfg_.rt_timeout) ||
        (!linger_after_stream_finish_ &&
         sender_.state() == TCPSender::State::kFinAcked)) {
        active_ = false;
    }
}

void TCPConnection::unclean_shutdown() {
    receiver_.stream_out().set_error();
    sender_.stream_in().set_error();
    linger_after_stream_finish_ = false;
    active_ = false;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // The connection is LISTEN.
    if (receiver_.state() == TCPReceiver::State::kListen &&
        sender_.state() == TCPSender::State::kClosed) {
        // Non-SYN or ACKs or RSTs in LISTEN should be ignored.
        if (!seg.header().syn || seg.header().ack || seg.header().rst) {
            return;
        }
    }
    ms_since_last_recv_ = 0;
    // RST: set both the inbound/outbound streams to the error state and kill the connection.
    if (seg.header().rst) {
        unclean_shutdown();
        return;
    }
    // Give the segment to the receiver.
    receiver_.segment_received(seg);
    // ACK: tell the sender about the fields it cares about.
    if (seg.header().ack) {
        sender_.ack_received(seg.header().ackno, seg.header().win);
    }
    // If read end is closed and write end is not closed, it is the passive close case.
    if (receiver_.state() == TCPReceiver::State::kFinRecv &&
        sender_.state() == TCPSender::State::kSynAcked) {
        linger_after_stream_finish_ = false;
        try_clean_shutdown();
    }
    // Try to send some segments.
    sender_.fill_window();
    // Maybe need to send empty segment to reply.
    size_t segment_length = seg.length_in_sequence_space();
    // case 1: If the incoming segment occupied any seq no, we need to send at least one segment in reply,
    //         to reflect an update in the ack no and window size.
    // case 2: The peer may send a segment with an invalid seq no for keep-alive.
    //         We should reply even though the segment does not occupy any seq no.
    if (segment_length > 0 ||
        (receiver_.state() == TCPReceiver::State::kSynRecv &&
         seg.header().seqno == receiver_.ackno().value() - 1)) {
        sender_.send_empty_segment();
    }
    try_clean_shutdown();
    enqueue_segments();
}

bool TCPConnection::active() const {
    return active_;
}

size_t TCPConnection::write(const std::string& data) {
    size_t write_size = sender_.stream_in().write(data);
    sender_.fill_window();
    enqueue_segments();
    return write_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    ms_since_last_recv_ += ms_since_last_tick;
    sender_.tick(ms_since_last_tick);
    if (sender_.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_shutdown();
        need_send_rst_ = true;
        sender_.send_empty_segment();
    } else {
        try_clean_shutdown();
    }
    enqueue_segments();
}

void TCPConnection::end_input_stream() {
    sender_.stream_in().end_input();
    sender_.fill_window();
    enqueue_segments();
}

void TCPConnection::connect() {
    sender_.fill_window();
    enqueue_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            need_send_rst_ = true;
            sender_.send_empty_segment();
            enqueue_segments();
        }
    } catch (const std::exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
