#include "tcp_segment.hh"

#include "parser.hh"
#include "util.hh"

#include <variant>

using namespace std;

//! \param[in] buffer string/Buffer to be parsed
//! \param[in] datagram_layer_checksum pseudo-checksum from the lower-layer protocol
ParseResult TCPSegment::parse(const Buffer buffer, const uint32_t datagram_layer_checksum) {
    InternetChecksum check(datagram_layer_checksum);
    check.add(buffer);
    if (check.value()) {
        return ParseResult::BadChecksum;
    }

    NetParser p{buffer};
    _header.parse(p);
    _payload = p.buffer();
    return p.get_error();
}

std::string TCPSegment::str() const {
    char buffer[128];
    size_t payload_size = _payload.size();
    snprintf(buffer, sizeof(buffer),
             "Segment(S=%d, A=%d, F=%d, R=%d, seq_no=%d, ack_no=%d) size %zd: %s...",
             _header.syn, _header.ack, _header.fin, _header.rst,
             _header.seqno.raw_value(), _header.ackno.raw_value(), payload_size,
             std::string(_payload.str().substr(0, std::min<size_t>(payload_size, 10))).c_str());
    return buffer;
}

size_t TCPSegment::length_in_sequence_space() const {
    return payload().str().size() + (header().syn ? 1 : 0) + (header().fin ? 1 : 0);
}

//! \param[in] datagram_layer_checksum pseudo-checksum from the lower-layer protocol
BufferList TCPSegment::serialize(const uint32_t datagram_layer_checksum) const {
    TCPHeader header_out = _header;
    header_out.cksum = 0;

    // calculate checksum -- taken over entire segment
    InternetChecksum check(datagram_layer_checksum);
    check.add(header_out.serialize());
    check.add(_payload);
    header_out.cksum = check.value();

    BufferList ret;
    ret.append(header_out.serialize());
    ret.append(_payload);

    return ret;
}
