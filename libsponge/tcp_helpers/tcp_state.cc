#include "tcp_state.hh"

using namespace std;

bool TCPState::operator==(const TCPState &other) const {
    return _active == other._active and _linger_after_streams_finish == other._linger_after_streams_finish and
           _sender == other._sender and _receiver == other._receiver;
}

bool TCPState::operator!=(const TCPState &other) const { return not operator==(other); }

string TCPState::name() const {
    return "sender=`" + _sender + "`, receiver=`" + _receiver + "`, active=" + to_string(_active) +
           ", linger_after_streams_finish=" + to_string(_linger_after_streams_finish);
}

TCPState::TCPState(const TCPState::State state) {
    switch (state) {
        case TCPState::State::LISTEN:
            _receiver = TCPReceiverStateSummary::LISTEN;
            _sender = TCPSenderStateSummary::CLOSED;
            break;
        case TCPState::State::SYN_RCVD:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::SYN_SENT;
            break;
        case TCPState::State::SYN_SENT:
            _receiver = TCPReceiverStateSummary::LISTEN;
            _sender = TCPSenderStateSummary::SYN_SENT;
            break;
        case TCPState::State::ESTABLISHED:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::SYN_ACKED;
            break;
        case TCPState::State::CLOSE_WAIT:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::SYN_ACKED;
            _linger_after_streams_finish = false;
            break;
        case TCPState::State::LAST_ACK:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_SENT;
            _linger_after_streams_finish = false;
            break;
        case TCPState::State::CLOSING:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_SENT;
            break;
        case TCPState::State::FIN_WAIT_1:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::FIN_SENT;
            break;
        case TCPState::State::FIN_WAIT_2:
            _receiver = TCPReceiverStateSummary::SYN_RECV;
            _sender = TCPSenderStateSummary::FIN_ACKED;
            break;
        case TCPState::State::TIME_WAIT:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_ACKED;
            break;
        case TCPState::State::RESET:
            _receiver = TCPReceiverStateSummary::ERROR;
            _sender = TCPSenderStateSummary::ERROR;
            _linger_after_streams_finish = false;
            _active = false;
            break;
        case TCPState::State::CLOSED:
            _receiver = TCPReceiverStateSummary::FIN_RECV;
            _sender = TCPSenderStateSummary::FIN_ACKED;
            _linger_after_streams_finish = false;
            _active = false;
            break;
    }
}

TCPState::TCPState(const TCPSender &sender, const TCPReceiver &receiver, const bool active, const bool linger)
    : _sender(state_summary(sender))
    , _receiver(state_summary(receiver))
    , _active(active)
    , _linger_after_streams_finish(active && linger) {}

string TCPState::state_summary(const TCPReceiver &receiver) {
    TCPReceiver::State state = receiver.state();
    if (state == TCPReceiver::State::kError) {
        return TCPReceiverStateSummary::ERROR;
    } else if (state == TCPReceiver::State::kListen) {
        return TCPReceiverStateSummary::LISTEN;
    } else if (state == TCPReceiver::State::kFinRecv) {
        return TCPReceiverStateSummary::FIN_RECV;
    } else {
        return TCPReceiverStateSummary::SYN_RECV;
    }
}

string TCPState::state_summary(const TCPSender &sender) {
    TCPSender::State state = sender.state();
    if (state == TCPSender::State::kError) {
        return TCPSenderStateSummary::ERROR;
    } else if (state == TCPSender::State::kClosed) {
        return TCPSenderStateSummary::CLOSED;
    } else if (state == TCPSender::State::kSynSent) {
        return TCPSenderStateSummary::SYN_SENT;
    } else if (state == TCPSender::State::kSynAcked) {
        return TCPSenderStateSummary::SYN_ACKED;
    } else if (state == TCPSender::State::kFinSent) {
        return TCPSenderStateSummary::FIN_SENT;
    } else {
        return TCPSenderStateSummary::FIN_ACKED;
    }
}
