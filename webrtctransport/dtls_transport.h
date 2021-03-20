//
// Created by xueyuegui on 19-12-7.
//

#ifndef MYWEBRTC_MYDTLSTRANSPORT_H
#define MYWEBRTC_MYDTLSTRANSPORT_H

#include <functional>
#include <memory>

#include "dtls/dtls_socket.h"
#include "srtp_channel.h"

class DtlsTransport : dtls::DtlsReceiver {
 public:
  typedef std::shared_ptr<DtlsTransport> Ptr;

  DtlsTransport(bool bServer);
  ~DtlsTransport();

  void Start();
  void Close();
  void InputData(char* buf, int len);
  void OutputData(char* buf, int len);
  static bool IsDtlsPacket(const char* buf, int len);
  std::string GetMyFingerprint() { return dtls_ctx_->getFingerprint(); };

  // override
  void onHandshakeCompleted(dtls::DtlsSocketContext* ctx, std::string clientKey,
                            std::string serverKey, std::string srtp_profile) override;
  void onHandshakeFailed(dtls::DtlsSocketContext* ctx, const std::string& error) override;
  void onDtlsPacket(dtls::DtlsSocketContext* ctx, const unsigned char* data,
                    unsigned int len) override;

  void SetHandshakeCompletedCB(
      std::function<void(std::string clientKey, std::string serverKey)> cb) {
    handshake_completed_callback_ = cb;
  }
  void SetHandshakeFailedCB(std::function<void()> cb) { handshake_failed_callback_ = cb; }
  void SetOutPutCB(std::function<void(char* buf, int len)> cb) { output_callbacke = cb; }

 private:
  std::shared_ptr<dtls::DtlsSocketContext> dtls_ctx_;
  std::function<void(std::string client_key, std::string server_key)> handshake_completed_callback_;
  std::function<void()> handshake_failed_callback_;
  std::function<void(char* buf, int len)> output_callbacke;
  bool is_server_ = false;
};

#endif  // MYWEBRTC_MYDTLSTRANSPORT_H
