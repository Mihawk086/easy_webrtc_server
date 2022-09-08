#pragma once

namespace webrtc {

enum class VideoFrameType {
  kEmptyFrame = 0,
  // Wire format for MultiplexEncodedImagePacker seems to depend on numerical
  // values of these constants.
  kVideoFrameKey = 3,
  kVideoFrameDelta = 4,
};

struct RtpVideoHeader {
  bool is_first_packet_in_frame = false;
  bool is_last_packet_in_frame = false;
  VideoFrameType frame_type = VideoFrameType::kEmptyFrame;
};

}  // namespace webrtc