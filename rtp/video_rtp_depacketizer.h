#pragma once

#include <optional>

#include "rtp/rtp_packet.h"
#include "rtp/rtp_video_header.h"

namespace webrtc {
class VideoRtpDepacketizer {
 public:
  struct ParsedRtpPayload {
    RtpPacket* packet;
    RtpVideoHeader video_header;
    size_t parsed_playload_offset = 0;
    size_t playload_size = 0;
    bool has_playload_header = false;
    std::vector<uint8_t> playload_header;
  };

  virtual ~VideoRtpDepacketizer() = default;
  virtual std::optional<ParsedRtpPayload> Parse(RtpPacket* packet) = 0;
  virtual void AssembleFrame();
};
}  // namespace webrtc