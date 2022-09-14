#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <optional>
#include <random>

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : isn_(fixed_isn.value_or(WrappingInt32{std::random_device()()}))
    , init_retransmission_timeout_{retx_timeout}
    , stream_(capacity)
    , timer_(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return bytes_in_flight_; }

void TCPSender::send_segment(TCPSegment& seg) {
    size_t seg_length = seg.length_in_sequence_space();
    next_seq_no_ += seg_length;
    bytes_in_flight_ += seg_length;
    segments_out_.emplace(seg);
    outstanding_segments_.emplace(std::move(seg));
    if (!timer_.started()) {
        timer_.restart();
    }
}

uint64_t TCPSender::free_window_size() const {
    // If the receiver has announced a window size of 0, and there is no
    // byte in flight, we should act like the window size is 1.
    if (window_size_ == 0 && next_seq_no_ == last_ack_no_) {
        return 1;
    }
    // There are some bytes in flight, but the window is full, we could not
    // send anymore. Just wait for tick() to trigger the retransmission.
    if (window_size_ <= next_seq_no_ - last_ack_no_) {
        return 0;
    }
    return window_size_ - (next_seq_no_ - last_ack_no_);
}

void TCPSender::fill_window() {
    // Initially the window size is 1, so only SYN flag can be sent.
    // CLOSE => SYN_SENT.
    if (state() == State::kClosed) {
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = isn_;
        send_segment(seg);
        return;
    }
    // Try to fill the window, as long as there are new bytes to be read
    // and space available in the window.
    while (true) {
        size_t free_window = free_window_size();
        if (free_window == 0) {
            return;
        }
        size_t stream_size = stream_.buffer_size();
        bool need_send_fin = stream_.input_ended() && state() == State::kSynAcked;
        if (stream_size == 0 && !need_send_fin) {
            return;
        }
        size_t send_size = std::min(std::min(stream_size, free_window), TCPConfig::MAX_PAYLOAD_SIZE);
        TCPSegment seg;
        seg.header().seqno = wrap(next_seq_no_, isn_);
        seg.payload() = stream_.read(send_size);
        // Only when the |stream_| is ended after being read, could we sent FIN.
        // SYN_ACKED => FIN_SENT.
        if (stream_.eof() && free_window > send_size && need_send_fin) {
            seg.header().fin = true;
        }
        send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t abs_ack_no = unwrap(ackno, isn_, last_ack_no_);
    // Ignore old / repeated ACKs and impossible ACKs (beyond next seq no).
    // Timer also does not restart without ACK of new data.
    if (abs_ack_no <= last_ack_no_ || abs_ack_no > next_seq_no_) {
        // Repeated ACKs as last time need to update the window size.
        if (abs_ack_no == last_ack_no_) {
            window_size_ = static_cast<uint64_t>(window_size);
        }
        return;
    }
    // RTO resets on ACK of new data.
    timer_.reset(init_retransmission_timeout_);
    retransmission_count_ = 0;
    // When all outstanding data has been acknowledged, keep the timer stopped.
    while (!outstanding_segments_.empty()) {
        const TCPSegment& seg = outstanding_segments_.front();
        uint64_t abs_seq_no = unwrap(seg.header().seqno, isn_, last_ack_no_);
        if (abs_seq_no + seg.length_in_sequence_space() > abs_ack_no) {
            // If the sender still has any outstanding data, restart the retransmission timer.
            timer_.restart();
            break;
        }
        // The front segment has been fully acknowledged.
        bytes_in_flight_ -= seg.length_in_sequence_space();
        outstanding_segments_.pop();
    }
    window_size_ = static_cast<uint64_t>(window_size);
    last_ack_no_ = abs_ack_no;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    timer_.tick(ms_since_last_tick);
    if (!timer_.expired()) {
        return;
    }
    // If there is no outstanding segment, the timer must be stopped and cannot expire.
    assert(!outstanding_segments_.empty());
    segments_out_.push(outstanding_segments_.front());
    // If window size is 0, we treat it as equal to 1 but don't back off RTO.
    if (window_size_ > 0) {
        retransmission_count_++;
        size_t rto = timer_.timeout_ms();
        timer_.reset(rto * 2);
    }
    timer_.restart();
}

unsigned int TCPSender::consecutive_retransmissions() const { return retransmission_count_; }

void TCPSender::send_empty_segment() {
    if (segments_out_.empty()) {
        TCPSegment seg;
        // Any outgoing segment needs to have the proper seq no.
        seg.header().seqno = wrap(next_seq_no_, isn_);
        segments_out_.emplace(std::move(seg));
    }
}

TCPSender::State TCPSender::state() const {
    if (stream_.error()) {
        return TCPSender::State::kError;
    }
    if (next_seq_no_ == 0) {
        return TCPSender::State::kClosed;
    }
    if (next_seq_no_ == bytes_in_flight_) {
        return TCPSender::State::kSynSent;
    }
    if (!stream_.eof() || next_seq_no_ < stream_.bytes_written() + 2) {
        return TCPSender::State::kSynAcked;
    }
    if (bytes_in_flight_) {
        return TCPSender::State::kFinSent;
    }
    return TCPSender::State::kFinAcked;
}
