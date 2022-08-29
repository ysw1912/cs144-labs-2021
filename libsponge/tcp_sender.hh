#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <cassert>
#include <functional>
#include <queue>

class Timer {
  public:
    explicit Timer(const size_t timeout_ms) : timeout_ms_(timeout_ms) {}

    size_t timeout_ms() const { return timeout_ms_; }

    bool started() const { return started_; }

    void restart() {
        reset();
        started_ = true;
    }

    void reset() {
        elapsed_ms_ = 0;
        started_ = false;
    }

    void reset(const size_t timeout_ms) {
        timeout_ms_ = timeout_ms;
        reset();
    }

    void tick(const size_t ms_since_last_tick) {
        if (started_) {
            elapsed_ms_ += ms_since_last_tick;
        }
    }

    bool expired() const { return started_ && elapsed_ms_ >= timeout_ms_; }

  private:
    size_t timeout_ms_;
    size_t elapsed_ms_ = 0;  // The time elapsed since the timer reset.
    bool started_ = false;
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! Initial sequence number.
    WrappingInt32 isn_;

    //! Outbound queue of segments that the TCPSender wants sent.
    std::queue<TCPSegment> segments_out_{};

    //! Segments have been sent but not yet acknowledged by the receiver.
    std::queue<TCPSegment> outstanding_segments_{};

    //! Initial retransmission timer for the connection.
    unsigned int init_retransmission_timeout_;

    //! Outgoing stream of bytes that have not yet been sent.
    ByteStream stream_;

    Timer timer_;

    size_t retransmission_count_ = 0;

    //! Absolute sequence number for the next byte to be sent.
    uint64_t next_seq_no_ = 0;

    //! Absolute ack number last received.
    uint64_t last_ack_no_ = 0;

    uint64_t bytes_in_flight_ = 0;

    uint64_t window_size_ = 1;

  private:
    bool syn_sent() const { return next_seq_no_ > 0; }

    bool fin_sent() const { return stream_.eof() && next_seq_no_ == stream_.bytes_written() + 2; }

    //! Free space in the receive window.
    uint64_t free_window_size() const;

    void send_segment(TCPSegment& seg);

  public:
    //! Initialize a TCPSender
    explicit TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
                       const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
                       const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return stream_; }
    const ByteStream &stream_in() const { return stream_; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    uint64_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment>& segments_out() { return segments_out_; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return next_seq_no_; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(next_seq_no_, isn_); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
