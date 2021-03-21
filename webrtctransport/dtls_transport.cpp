//
// Created by xueyuegui on 19-12-7.
//

#include "dtls_transport.h"

#include <iostream>

static std::string Base64Encode(const std::string &input, bool with_new_line) {
  BIO *bmem = NULL;
  BIO *b64 = NULL;
  BUF_MEM *bptr = NULL;
  b64 = BIO_new(BIO_f_base64());
  if (!with_new_line) {
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  }
  bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);
  BIO_write(b64, input.c_str(), input.length());
  BIO_flush(b64);
  BIO_get_mem_ptr(b64, &bptr);
  std::string result(bptr->data, bptr->length);
  BIO_free_all(b64);
  return result;
}

DtlsTransport::DtlsTransport(bool is_server) : is_server_(is_server) {
  dtls_transport_.reset(new RTC::DtlsTransport(this));
}

DtlsTransport::~DtlsTransport() {}

void DtlsTransport::Start() {
  if (is_server_) {
    dtls_transport_->Run(RTC::DtlsTransport::Role::SERVER);
  } else {
    dtls_transport_->Run(RTC::DtlsTransport::Role::CLIENT);
  }
  return;
}

void DtlsTransport::Close() {}

void DtlsTransport::OnDtlsTransportConnecting(const RTC::DtlsTransport *dtlsTransport) {}

void DtlsTransport::OnDtlsTransportConnected(const RTC::DtlsTransport *dtlsTransport,
                                             RTC::CryptoSuite srtpCryptoSuite,
                                             uint8_t *srtpLocalKey, size_t srtpLocalKeyLen,
                                             uint8_t *srtpRemoteKey, size_t srtpRemoteKeyLen,
                                             std::string &remoteCert) {
  std::cout << "dtls successful" << std::endl;
  std::string client_key;
  std::string server_key;
  server_key.assign((char *)srtpLocalKey, srtpLocalKeyLen);
  client_key.assign((char *)srtpRemoteKey, srtpRemoteKeyLen);
  client_key = Base64Encode(client_key, false);
  server_key = Base64Encode(server_key, false);
  if (is_server_) {
    // If we are server, we swap the keys
    client_key.swap(server_key);
  }
  if (handshake_completed_callback_) {
    handshake_completed_callback_(client_key, server_key);
  }
}

void DtlsTransport::OnDtlsTransportFailed(const RTC::DtlsTransport *dtlsTransport) {
  if (handshake_failed_callback_) {
    handshake_failed_callback_();
  }
}

void DtlsTransport::OnDtlsTransportClosed(const RTC::DtlsTransport *dtlsTransport) {}

void DtlsTransport::OnDtlsTransportSendData(const RTC::DtlsTransport *dtlsTransport,
                                            const uint8_t *data, size_t len) {
  if (output_callback_) {
    output_callback_((char *)data, len);
  }
}

void DtlsTransport::OutputData(char *buf, int len) {
  if (output_callback_) {
    output_callback_(buf, len);
  }
}

void DtlsTransport::OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport *dtlsTransport,
                                                           const uint8_t *data, size_t len) {}

bool DtlsTransport::IsDtlsPacket(const char *buf, int len) {
  return RTC::DtlsTransport::IsDtls((uint8_t *)buf, len);
}

void DtlsTransport::InputData(char *buf, int len) {
  dtls_transport_->ProcessDtlsData((uint8_t *)buf, len);
}
