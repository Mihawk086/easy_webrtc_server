#pragma once
#include <optional>

#include "rtp/video_rtp_depacketizer.h"

namespace webrtc {
class VideoRtpDepacketizerH264 : public VideoRtpDepacketizer {
 public:
  ~VideoRtpDepacketizerH264() override = default;

  std::optional<ParsedRtpPayload> Parse(RtpPacket* packet) override;
};
}  // namespace webrtc