#pragma once

#include <cstddef>
#include <cstdint>

namespace webrtc {

bool IsRtpPacket(const uint8_t* packet, size_t size);

bool IsRtcpPacket(const uint8_t* packet, size_t size);

int ParseRtpPayloadType(const uint8_t* rtp_packet);

uint16_t ParseRtpSequenceNumber(const uint8_t* rtp_packet);

uint32_t ParseRtpSsrc(const uint8_t* rtp_packet);

}