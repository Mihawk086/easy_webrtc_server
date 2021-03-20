#pragma once

#include <functional>

class RtpMaker {
 public:
  RtpMaker();
  ~RtpMaker();
  void InputH264Frame(char* buf, int len, uint32_t timestamp);
  void SetRtpCallBack(std::function<void(char* buf, int len)> cb) {
    make_rtp_completed_callback = cb;
  };
  void Setssrc(uint32_t ssrc) { ssrc_ = ssrc; };

 private:
  std::function<void(char* buf, int len)> make_rtp_completed_callback;
  char buf_[5000];
  uint16_t seq_ = 0;
  uint32_t ssrc_ = 12345678;
};
