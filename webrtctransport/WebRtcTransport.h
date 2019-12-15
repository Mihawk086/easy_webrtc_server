//
// Created by xueyuegui on 19-12-7.
//

#ifndef MYWEBRTC_WEBRTCTRANSPORT_H
#define MYWEBRTC_WEBRTCTRANSPORT_H

#include <memory>
#include <string>

#include "MyDtlsTransport.h"
#include "RtpMaker.h"
#include "IceServer.h"
#include "UdpSocket.h"
#include "StunPacket.hpp"
#include "net/EventLoop.h"

namespace erizo {
    class WebRtcTransport :public std::enable_shared_from_this<WebRtcTransport>{
    public:
        WebRtcTransport(xop::EventLoop* loop);
        ~WebRtcTransport();

        void Start();
        std::string GetLocalSdp();
        void OnIceServerCompleted();
        void OnDtlsCompleted(std::string clientKey, std::string serverKey);

        void onInputDataPacket(char* buf ,int len ,struct sockaddr_in* remoteAddr);
        void WritePacket(char* buf ,int len,struct sockaddr_in* remoteAddr);
        void WritePacket(char* buf ,int len);
        void WritRtpPacket(char* buf , int len);
        void WriteH264Frame(char* buf ,int len, uint32_t timestamp);
    private:
        UdpSocket::Ptr m_pUdpSocket;
        IceServer::Ptr m_IceServer;
        MyDtlsTransport::Ptr m_pDtlsTransport;
        std::shared_ptr<SrtpChannel> m_Srtp;

        char m_ProtectBuf[65536];
        bool m_bReady;
        std::string m_strIP;
        int16_t m_nPort;
        xop::EventLoop* m_loop;
        struct sockaddr_in m_RemoteSockaddr;
        RtpMaker m_rtpmaker;
    };
}

#endif //MYWEBRTC_WEBRTCTRANSPORT_H
