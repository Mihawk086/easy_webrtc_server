#pragma once

#include <thread>
#include <memory>
#include <atomic>
#include <list>

namespace erizo {
    class WebRtcTransport;
}
class FFmpegSrc
{
public:
    static FFmpegSrc* GetInsatance();
	~FFmpegSrc();
    void InputH264(char* pcData, int iDataLen, uint32_t timestamp);
    void Start();
    void Stop();
    void ThreadEntry();
    void AddClient(std::weak_ptr<erizo::WebRtcTransport> client);
private:
    FFmpegSrc();
    std::shared_ptr<std::thread> m_pThread;
    std::atomic<bool> m_bStart;
    std::list<std::weak_ptr<erizo::WebRtcTransport>> m_clients;
};