#include "rtp/video_rtp_depacketizer_h264.h"

#include "rtp/byte_io.h"

namespace webrtc {

namespace {

namespace H264 {
const size_t kNaluTypeSize = 1;
enum NaluType : uint8_t {
  kSlice = 1,
  kIdr = 5,
  kSei = 6,
  kSps = 7,
  kPps = 8,
  kAud = 9,
  kEndOfSequence = 10,
  kEndOfStream = 11,
  kFiller = 12,
  kPrefix = 14,
  kStapA = 24,
  kFuA = 28
};
}  // namespace H264

enum H264PacketizationTypes {
  kH264SingleNalu,  // This packet contains a single NAL unit.
  kH264StapA,       // This packet contains STAP-A (single time
                    // aggregation) packets. If this packet has an
                    // associated NAL unit type, it'll be for the
                    // first such aggregated packet.
  kH264FuA,         // This packet contains a FU-A (fragmentation
                    // unit) packet, meaning it is a part of a frame
                    // that was too large to fit into a single packet.
};

constexpr size_t kNalHeaderSize = 1;
constexpr size_t kFuAHeaderSize = 2;
constexpr size_t kLengthFieldSize = 2;
constexpr size_t kStapAHeaderSize = kNalHeaderSize + kLengthFieldSize;

// Bit masks for FU (A and B) indicators.
enum NalDefs : uint8_t { kFBit = 0x80, kNriMask = 0x60, kTypeMask = 0x1F };

// Bit masks for FU (A and B) headers.
enum FuDefs : uint8_t { kSBit = 0x80, kEBit = 0x40, kRBit = 0x20 };

bool ParseStapAStartOffsets(const uint8_t* nalu_ptr,
                            size_t length_remaining,
                            std::vector<size_t>* offsets) {
  size_t offset = 0;
  while (length_remaining > 0) {
    // Buffer doesn't contain room for additional nalu length.
    if (length_remaining < sizeof(uint16_t))
      return false;
    uint16_t nalu_size = ByteReader<uint16_t>::ReadBigEndian(nalu_ptr);
    nalu_ptr += sizeof(uint16_t);
    length_remaining -= sizeof(uint16_t);
    if (nalu_size > length_remaining)
      return false;
    nalu_ptr += nalu_size;
    length_remaining -= nalu_size;

    offsets->push_back(offset + kStapAHeaderSize);
    offset += kLengthFieldSize + nalu_size;
  }
  return true;
}

std::optional<VideoRtpDepacketizer::ParsedRtpPayload> ProcessStapAOrSingleNalu(
    RtpPacket* packet) {
  const uint8_t* const payload_data = packet->payload();
  VideoRtpDepacketizer::ParsedRtpPayload parsed_payload;
  bool modified_buffer = false;

  const uint8_t* nalu_start = payload_data + kNalHeaderSize;
  const size_t nalu_length = packet->payload_size() - kNalHeaderSize;
  uint8_t nal_type = payload_data[0] & kTypeMask;
  std::vector<size_t> nalu_start_offsets;
  if (nal_type == H264::NaluType::kStapA) {
    // Skip the StapA header (StapA NAL type + length).
    if (packet->payload_size() <= kStapAHeaderSize) {
      return std::nullopt;
    }
    if (!ParseStapAStartOffsets(nalu_start, nalu_length, &nalu_start_offsets)) {
      return std::nullopt;
    }
    nal_type = payload_data[kStapAHeaderSize] & kTypeMask;
  } else {
    nalu_start_offsets.push_back(0);
  }

  parsed_payload.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  nalu_start_offsets.push_back(packet->payload_size() +
                               kLengthFieldSize);  // End offset.
  for (size_t i = 0; i < nalu_start_offsets.size() - 1; ++i) {
    size_t start_offset = nalu_start_offsets[i];
    // End offset is actually start offset for next unit, excluding length field
    // so remove that from this units length.
    size_t end_offset = nalu_start_offsets[i + 1] - kLengthFieldSize;
    if (end_offset - start_offset < H264::kNaluTypeSize) {
      return std::nullopt;
    }
    uint8_t type = payload_data[start_offset] & kTypeMask;
    start_offset += H264::kNaluTypeSize;

    switch (type) {
      case H264::NaluType::kSps: {
        
      }
      case H264::NaluType::kPps: {
        break;
      }
      case H264::NaluType::kIdr: {
        break;
      }
      // Slices below don't contain SPS or PPS ids.
      case H264::NaluType::kAud:
      case H264::NaluType::kEndOfSequence:
      case H264::NaluType::kEndOfStream:
      case H264::NaluType::kFiller:
      case H264::NaluType::kSei:
        break;
      case H264::NaluType::kStapA:
      case H264::NaluType::kFuA:
        return std::nullopt;
    }
  }

  return parsed_payload;
}

std::optional<VideoRtpDepacketizer::ParsedRtpPayload> ParseFuaNalu(
    RtpPacket* packet) {
  if (packet->payload_size() < kFuAHeaderSize) {
    return std::nullopt;
  }
  VideoRtpDepacketizer::ParsedRtpPayload parsed_payload;
  parsed_payload.packet = packet;
  const uint8_t* rtp_payload = packet->payload();
  uint8_t fnri = rtp_payload[0] & (kFBit | kNriMask);
  uint8_t original_nal_type = rtp_payload[1] & kTypeMask;
  bool first_fragment = (rtp_payload[1] & kSBit) > 0;
  if (first_fragment) {
    uint8_t original_nal_header = fnri | original_nal_type;
    parsed_payload.has_playload_header = true;
    parsed_payload.playload_header.push_back(original_nal_header);
    parsed_payload.parsed_playload_offset = kFuAHeaderSize;
    parsed_payload.playload_size = packet->payload_size() - kFuAHeaderSize;
  } else {
    parsed_payload.has_playload_header = false;
    parsed_payload.parsed_playload_offset = kFuAHeaderSize;
    parsed_payload.playload_size = packet->payload_size() - kFuAHeaderSize;
  }

  if (original_nal_type == H264::NaluType::kIdr) {
    parsed_payload.video_header.frame_type = VideoFrameType::kVideoFrameKey;
  } else {
    parsed_payload.video_header.frame_type = VideoFrameType::kVideoFrameDelta;
  }
  parsed_payload.video_header.is_first_packet_in_frame = first_fragment;
  return parsed_payload;
}

}  // namespace

std::optional<VideoRtpDepacketizer::ParsedRtpPayload>
VideoRtpDepacketizerH264::Parse(RtpPacket* packet) {
  if (packet->payload_size() == 0) {
    return std::nullopt;
  }

  uint8_t nal_type = packet->payload()[0] & kTypeMask;

  if (nal_type == H264::NaluType::kFuA) {
    // Fragmented NAL units (FU-A).
    return ParseFuaNalu(packet);
  } else {
    // We handle STAP-A and single NALU's the same way here. The jitter buffer
    // will depacketize the STAP-A into NAL units later.
    return ProcessStapAOrSingleNalu(packet);
  }
}
}  // namespace webrtc