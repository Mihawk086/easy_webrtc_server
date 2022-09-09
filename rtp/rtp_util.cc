#include "rtp/rtp_util.h"

#include "rtp/byte_io.h"

namespace webrtc {
namespace {

constexpr uint8_t kRtpVersion = 2;
constexpr size_t kMinRtpPacketLen = 12;
constexpr size_t kMinRtcpPacketLen = 4;

bool HasCorrectRtpVersion(const uint8_t* packet) {
  return packet[0] >> 6 == kRtpVersion;
}

// For additional details, see http://tools.ietf.org/html/rfc5761#section-4
bool PayloadTypeIsReservedForRtcp(uint8_t payload_type) {
  return 64 <= payload_type && payload_type < 96;
}

}  // namespace

bool IsRtpPacket(const uint8_t* packet, size_t size) {
  return size >= kMinRtpPacketLen && HasCorrectRtpVersion(packet) &&
         !PayloadTypeIsReservedForRtcp(packet[1] & 0x7F);
}

bool IsRtcpPacket(const uint8_t* packet, size_t size) {
  return size >= kMinRtcpPacketLen && HasCorrectRtpVersion(packet) &&
         PayloadTypeIsReservedForRtcp(packet[1] & 0x7F);
}

int ParseRtpPayloadType(const uint8_t* rtp_packet) {
  return rtp_packet[1] & 0x7F;
}

uint16_t ParseRtpSequenceNumber(const uint8_t* rtp_packet) {
  return ByteReader<uint16_t>::ReadBigEndian(rtp_packet + 2);
}

uint32_t ParseRtpSsrc(const uint8_t* rtp_packet) {
  return ByteReader<uint32_t>::ReadBigEndian(rtp_packet + 8);
}

}  // namespace webrtc