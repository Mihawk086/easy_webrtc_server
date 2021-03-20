//
// Created by xueyuegui on 19-12-7.
//

#include "dtls_transport.h"

using namespace erizo;
using dtls::DtlsSocketContext;

DtlsTransport::DtlsTransport(bool is_server) : is_server_(is_server) {
  dtls_ctx_.reset(new DtlsSocketContext());
  if (is_server) {
    dtls_ctx_->createServer();
  } else {
    dtls_ctx_->createClient();
  }
  dtls_ctx_->setDtlsReceiver(this);
}

DtlsTransport::~DtlsTransport() {}

void DtlsTransport::Start() {
  if (!is_server_) {
    dtls_ctx_->start();
  }
}

void DtlsTransport::Close() {}

void DtlsTransport::onHandshakeCompleted(dtls::DtlsSocketContext *ctx, std::string client_key,
                                         std::string server_key, std::string srtp_profile) {
  if (is_server_) {
    // If we are server, we swap the keys
    client_key.swap(server_key);
  }
  if (handshake_completed_callback_) {
    handshake_completed_callback_(client_key, server_key);
  }
}

void DtlsTransport::onHandshakeFailed(dtls::DtlsSocketContext *ctx, const std::string &error) {
  if (handshake_failed_callback_) {
    handshake_failed_callback_();
  }
}

void DtlsTransport::onDtlsPacket(dtls::DtlsSocketContext *ctx, const unsigned char *data,
                                 unsigned int len) {
  OutputData((char *)data, len);
}

bool DtlsTransport::IsDtlsPacket(const char *buf, int len) {
  int data = DtlsSocketContext::demuxPacket(reinterpret_cast<const unsigned char *>(buf), len);
  switch (data) {
    case DtlsSocketContext::dtls:
      return true;
      break;
    default:
      return false;
      break;
  }
}

void DtlsTransport::InputData(char *buf, int len) { dtls_ctx_->read((unsigned char *)buf, len); }

void DtlsTransport::OutputData(char *buf, int len) {
  if (output_callbacke) {
    output_callbacke(buf, len);
  }
}
