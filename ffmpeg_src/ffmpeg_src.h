#pragma once

#include <atomic>
#include <list>
#include <memory>
#include <thread>

class WebRtcTransport;

class FFmpegSrc {
 public:
  static FFmpegSrc* GetInsatance();
  ~FFmpegSrc();
  void InputH264(char* data, int len, uint32_t timestamp);
  void Start();
  void Stop();
  void ThreadEntry();
  void AddClient(std::weak_ptr<WebRtcTransport>);

 private:
  FFmpegSrc();
  std::shared_ptr<std::thread> thread_;
  std::atomic<bool> is_start_;
  std::list<std::weak_ptr<WebRtcTransport>> clients_;
};