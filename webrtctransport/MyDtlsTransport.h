//
// Created by xueyuegui on 19-12-7.
//

#ifndef MYWEBRTC_MYDTLSTRANSPORT_H
#define MYWEBRTC_MYDTLSTRANSPORT_H

#include <memory>
#include <functional>

#include "dtls/DtlsSocket.h"
#include "SrtpChannel.h"
#include "RtpHeaders.h"

namespace erizo {

    class MyDtlsTransport : dtls::DtlsReceiver{
    public:
        typedef std::shared_ptr<MyDtlsTransport>  Ptr ;

        MyDtlsTransport(bool bServer);
        ~MyDtlsTransport();

        void Start();
        void Close();
        void InputData(char* buf,int len);
        void OutputData(char* buf,int len);
        static bool isDtlsPacket(const char* buf, int len);
        std::string GetMyFingerprint(){return m_pDtls->getFingerprint();};

        //override
        void onHandshakeCompleted(dtls::DtlsSocketContext *ctx, std::string clientKey, std::string serverKey,
                                  std::string srtp_profile) override;
        void onHandshakeFailed(dtls::DtlsSocketContext *ctx, const std::string& error) override;
        void onDtlsPacket(dtls::DtlsSocketContext *ctx, const unsigned char* data, unsigned int len) override;

        void SetHandshakeCompletedCB(std::function<void(std::string clientKey, std::string serverKey)> cb){m_HandshakeCompletedCB = cb;}
        void SetHandshakeFailedCB(std::function<void()> cb){m_HandshakeFailedCB = cb;}
        void SetOutPutCB(std::function<void(char* buf, int len)> cb){m_OutPutCB = cb;}
    private:
        std::shared_ptr<dtls::DtlsSocketContext> m_pDtls;
        std::function<void(std::string clientKey, std::string serverKey)> m_HandshakeCompletedCB;
        std::function<void()> m_HandshakeFailedCB;
        std::function<void(char* buf, int len)> m_OutPutCB;
		bool m_bServer = false;
    };
}

#endif //MYWEBRTC_MYDTLSTRANSPORT_H
