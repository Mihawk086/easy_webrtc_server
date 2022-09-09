
#include "rtp/rtp_packet.h"

#include <cstring>
#include <sstream>

#include "rtp/byte_io.h"
namespace webrtc {

namespace {
constexpr size_t kFixedHeaderSize = 12;
constexpr uint8_t kRtpVersion = 2;
constexpr uint16_t kOneByteExtensionProfileId = 0xBEDE;
constexpr uint16_t kTwoByteExtensionProfileId = 0x1000;
constexpr uint16_t kTwobyteExtensionProfileIdAppBitsFilter = 0xfff0;
constexpr size_t kOneByteExtensionHeaderLength = 1;
constexpr size_t kTwoByteExtensionHeaderLength = 2;
constexpr size_t kDefaultPacketSize = 1500;
}  // namespace

//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=2|P|X|  CC   |M|     PT      |       sequence number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           timestamp                           |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           synchronization source (SSRC) identifier            |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |            Contributing source (CSRC) identifiers             |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |  header eXtension profile id  |       length in 32bits        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                          Extensions                           |
// |                             ....                              |
// +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// |                           Payload                             |
// |             ....              :  padding...                   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |               padding         | Padding size  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

bool RtpPacket::Parse(const uint8_t* buffer, size_t buffer_size) {
  if (!ParseBuffer(buffer, buffer_size)) {
    Clear();
    return false;
  }
  buffer_.resize(buffer_size);
  ::memcpy(&buffer_[0], buffer, buffer_size);
}

void RtpPacket::Clear() {
  marker_ = false;
  payload_type_ = 0;
  sequence_number_ = 0;
  timestamp_ = 0;
  ssrc_ = 0;
  payload_offset_ = kFixedHeaderSize;
  payload_size_ = 0;
  padding_size_ = 0;
}

std::string RtpPacket::ToString() const {
  std::stringstream result;
  result << "{payload_type=" << std::to_string(payload_type_)
         << "marker=" << marker_ << ", sequence_number=" << sequence_number_
         << ", padding_size=" << padding_size_ << ", timestamp=" << timestamp_
         << ", ssrc=" << ssrc_ << ", payload_offset=" << payload_offset_
         << ", payload_size=" << payload_size_ << ", total_size=" << size()
         << "}";
  return result.str();
}

bool RtpPacket::ParseBuffer(const uint8_t* buffer, size_t size) {
  if (size < kFixedHeaderSize) {
    return false;
  }
  const uint8_t version = buffer[0] >> 6;
  if (version != kRtpVersion) {
    return false;
  }
  const bool has_padding = (buffer[0] & 0x20) != 0;
  const bool has_extension = (buffer[0] & 0x10) != 0;
  const uint8_t number_of_crcs = buffer[0] & 0x0f;
  marker_ = (buffer[1] & 0x80) != 0;
  payload_type_ = buffer[1] & 0x7f;

  sequence_number_ = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);
  timestamp_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[4]);
  ssrc_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[8]);
  if (size < kFixedHeaderSize + number_of_crcs * 4) {
    return false;
  }
  payload_offset_ = kFixedHeaderSize + number_of_crcs * 4;

  if (has_extension) {
    /* RTP header extension, RFC 3550.
     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |      defined by profile       |           length              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                        header extension                       |
    |                             ....                              |
    */
    size_t extension_offset = payload_offset_ + 4;
    if (extension_offset > size) {
      return false;
    }
    uint16_t profile =
        ByteReader<uint16_t>::ReadBigEndian(&buffer[payload_offset_]);
    size_t extensions_capacity =
        ByteReader<uint16_t>::ReadBigEndian(&buffer[payload_offset_ + 2]);
    extensions_capacity *= 4;
    if (extension_offset + extensions_capacity > size) {
      return false;
    }
    if (profile != kOneByteExtensionProfileId &&
        (profile & kTwobyteExtensionProfileIdAppBitsFilter) !=
            kTwoByteExtensionProfileId) {
      // RTC_LOG(LS_WARNING) << "Unsupported rtp extension " << profile;
    } else {
    }
    payload_offset_ = extension_offset + extensions_capacity;
  }

  if (has_padding && payload_offset_ < size) {
    padding_size_ = buffer[size - 1];
    if (padding_size_ == 0) {
      // RTC_LOG(LS_WARNING) << "Padding was set, but padding size is zero";
      return false;
    }
  } else {
    padding_size_ = 0;
  }

  if (payload_offset_ + padding_size_ > size) {
    return false;
  }
  payload_size_ = size - payload_offset_ - padding_size_;
  return true;
}

}  // namespace webrtc