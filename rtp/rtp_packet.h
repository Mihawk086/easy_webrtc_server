#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace webrtc {

class RtpPacket {
 public:
  RtpPacket() = default;
  ~RtpPacket() = default;
  bool Parse(const uint8_t* buffer, size_t size);
  void Clear();
  std::string ToString() const;

  bool marker() const { return marker_; }
  uint8_t payload_type() const { return payload_type_; }
  uint16_t sequence_number() const { return sequence_number_; }
  uint32_t timestamp() const { return timestamp_; }
  uint32_t ssrc() const { return ssrc_; }
  size_t headers_size() const { return payload_offset_; }
  size_t payload_size() const { return payload_size_; }
  bool has_padding() const { return buffer_[0] & 0x20; }
  size_t padding_size() const { return padding_size_; }
  const uint8_t* payload() const { return &buffer_[0] + payload_offset_; }
  size_t size() const {
    return payload_offset_ + payload_size_ + padding_size_;
  }

 private:
  bool ParseBuffer(const uint8_t* buffer, size_t size);

  bool marker_;
  uint8_t payload_type_;
  uint8_t padding_size_;
  uint16_t sequence_number_;
  uint32_t timestamp_;
  uint32_t ssrc_;
  size_t payload_offset_;  // Match header size with csrcs and extensions.
  size_t payload_size_;
  std::vector<uint8_t> buffer_;
};

}  // namespace webrtc