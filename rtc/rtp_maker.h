#pragma once

#include <functional>

class RtpMaker {
 public:
  class Listener {
   public:
    virtual void SendRtpData(char* buf, int len) = 0;
  };

 public:
  RtpMaker(Listener* listener);
  ~RtpMaker();
  void InputH264Frame(char* buf, int len, uint32_t timestamp);
  void Setssrc(uint32_t ssrc) { ssrc_ = ssrc; };

 private:
  Listener* listener_;
  char buf_[5000];
  uint16_t seq_ = 0;
  uint32_t ssrc_ = 12345678;
};
