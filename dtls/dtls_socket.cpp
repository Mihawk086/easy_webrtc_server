#include "dtls/dtls_socket.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

#include "bf_dwrap.h"

using dtls::DtlsSocket;
using dtls::SrtpSessionKeys;
using std::memcpy;

int dummy_cb(int d, X509_STORE_CTX* x) { return 1; }

DtlsSocket::DtlsSocket(DtlsSocketContext* socketContext, enum SocketType type)
    : dtls_socket_ctx_(socketContext), socket_type_(type), is_handshake_completted(false) {
  ELOG_DEBUG("Creating Dtls Socket");
  dtls_socket_ctx_->setDtlsSocket(this);
  SSL_CTX* ssl_ctx_ = dtls_socket_ctx_->getSSLContext();
  assert(ssl_ctx_);
  ssl_ = SSL_new(ssl_ctx_);
  assert(ssl_ != 0);
  SSL_set_mtu(ssl_, DTLS_MTU);
  ssl_->ctx = ssl_ctx_;
  ssl_->session_ctx = ssl_ctx_;

  switch (type) {
    case Client:
      SSL_set_connect_state(ssl_);
      // SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE |
      //         SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
      break;
    case Server:
      SSL_set_accept_state(ssl_);
      SSL_set_verify(ssl_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, dummy_cb);
      break;
    default:
      assert(0);
  }
  BIO* memBIO1 = BIO_new(BIO_s_mem());
  in_bio_ = BIO_new(BIO_f_dwrap());
  BIO_push(in_bio_, memBIO1);

  BIO* memBIO2 = BIO_new(BIO_s_mem());
  out_bio_ = BIO_new(BIO_f_dwrap());
  BIO_push(out_bio_, memBIO2);

  SSL_set_bio(ssl_, in_bio_, out_bio_);
  SSL_accept(ssl_);
  ELOG_DEBUG("Dtls Socket created");
}

DtlsSocket::~DtlsSocket() { close(); }

void DtlsSocket::close() {
  // Properly shutdown the socket and free it - note: this also free's the BIO's
  if (ssl_ != NULL) {
    ELOG_DEBUG("SSL Shutdown");
    SSL_shutdown(ssl_);
    SSL_free(ssl_);
    ssl_ = NULL;
  }
}

void DtlsSocket::startClient() {
  assert(socket_type_ == Client);
  doHandshakeIteration();
}

bool DtlsSocket::handlePacketMaybe(const unsigned char* bytes, unsigned int len) {
  if (ssl_ == NULL) {
    ELOG_WARN("handlePacketMaybe called after DtlsSocket closed: %p", this);
    return false;
  }
  DtlsSocketContext::PacketType pType = DtlsSocketContext::demuxPacket(bytes, len);

  if (pType != DtlsSocketContext::dtls) {
    return false;
  }

  if (ssl_ == nullptr) {
    return false;
  }

  (void)BIO_reset(in_bio_);
  (void)BIO_reset(out_bio_);

  int r = BIO_write(in_bio_, bytes, len);
  assert(r == static_cast<int>(len));  // Can't happen

  // Note: we must catch any below exceptions--if there are any
  try {
    doHandshakeIteration();
  } catch (int e) {
    return false;
  }
  return true;
}

void DtlsSocket::forceRetransmit() {
  (void)BIO_reset(in_bio_);
  (void)BIO_reset(out_bio_);
  BIO_ctrl(in_bio_, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, 0);

  doHandshakeIteration();
}

void DtlsSocket::doHandshakeIteration() {
  std::lock_guard<std::mutex> lock(handshake_mutex_);
  char errbuf[1024];
  int sslerr;

  if (is_handshake_completted) return;

  int r = SSL_do_handshake(ssl_);
  errbuf[0] = 0;
  ERR_error_string_n(ERR_peek_error(), errbuf, sizeof(errbuf));

  // See what was written
  unsigned char* outBioData;
  int outBioLen = BIO_get_mem_data(out_bio_, &outBioData);
  if (outBioLen > DTLS_MTU) {
    ELOG_WARN("message: BIO data bigger than MTU - packet could be lost, outBioLen %u, MTU %u",
              outBioLen, DTLS_MTU);
  }

  // Now handle handshake errors */
  switch (sslerr = SSL_get_error(ssl_, r)) {
    case SSL_ERROR_NONE:
      is_handshake_completted = true;
      dtls_socket_ctx_->handshakeCompleted();
      break;
    case SSL_ERROR_WANT_READ:
      break;
    default:
      ELOG_ERROR("SSL error %d", sslerr);

      dtls_socket_ctx_->handshakeFailed(errbuf);
      // Note: need to fall through to propagate alerts, if any
      break;
  }

  // If out_bio_ is now nonzero-length, then we need to write the
  // data to the network. TODO(pedro): warning, MTU issues!
  if (outBioLen) {
    dtls_socket_ctx_->write(outBioData, outBioLen);
  }
}

bool DtlsSocket::getRemoteFingerprint(char* fprint) {
  X509* x = SSL_get_peer_certificate(ssl_);
  if (!x) {  // No certificate
    return false;
  }

  computeFingerprint(x, fprint);
  X509_free(x);
  return true;
}

bool DtlsSocket::checkFingerprint(const char* fingerprint, unsigned int len) {
  char fprint[100];

  if (getRemoteFingerprint(fprint) == false) {
    return false;
  }

  // used to be strncasecmp
  if (strncmp(fprint, fingerprint, len)) {
    ELOG_WARN("Fingerprint mismatch, got %s expecting %s", fprint, fingerprint);
    return false;
  }

  return true;
}

void DtlsSocket::getMyCertFingerprint(char* fingerprint) {
  dtls_socket_ctx_->getMyCertFingerprint(fingerprint);
}

SrtpSessionKeys* DtlsSocket::getSrtpSessionKeys() {
  // TODO(pedro): probably an exception candidate
  assert(is_handshake_completted);

  SrtpSessionKeys* keys = new SrtpSessionKeys();

  unsigned char material[SRTP_MASTER_KEY_LEN << 1];
  if (!SSL_export_keying_material(ssl_, material, sizeof(material), "EXTRACTOR-dtls_srtp", 19, NULL,
                                  0, 0)) {
    return keys;
  }

  size_t offset = 0;

  memcpy(keys->clientMasterKey, &material[offset], SRTP_MASTER_KEY_KEY_LEN);
  offset += SRTP_MASTER_KEY_KEY_LEN;
  memcpy(keys->serverMasterKey, &material[offset], SRTP_MASTER_KEY_KEY_LEN);
  offset += SRTP_MASTER_KEY_KEY_LEN;
  memcpy(keys->clientMasterSalt, &material[offset], SRTP_MASTER_KEY_SALT_LEN);
  offset += SRTP_MASTER_KEY_SALT_LEN;
  memcpy(keys->serverMasterSalt, &material[offset], SRTP_MASTER_KEY_SALT_LEN);
  offset += SRTP_MASTER_KEY_SALT_LEN;
  keys->clientMasterKeyLen = SRTP_MASTER_KEY_KEY_LEN;
  keys->serverMasterKeyLen = SRTP_MASTER_KEY_KEY_LEN;
  keys->clientMasterSaltLen = SRTP_MASTER_KEY_SALT_LEN;
  keys->serverMasterSaltLen = SRTP_MASTER_KEY_SALT_LEN;

  return keys;
}

SRTP_PROTECTION_PROFILE* DtlsSocket::getSrtpProfile() {
  // TODO(pedro): probably an exception candidate
  assert(is_handshake_completted);
  return SSL_get_selected_srtp_profile(ssl_);
}

// Fingerprint is assumed to be long enough
void DtlsSocket::computeFingerprint(X509* cert, char* fingerprint) {
  unsigned char md[EVP_MAX_MD_SIZE];
  int r;
  unsigned int i, n;

  // r = X509_digest(cert, EVP_sha1(), md, &n);
  r = X509_digest(cert, EVP_sha256(), md, &n);
  // TODO(javier) - is sha1 vs sha256 supposed to come from DTLS handshake?
  // fixing to to SHA-256 for compatibility with current web-rtc implementations
  assert(r == 1);

  for (i = 0; i < n; i++) {
    sprintf(fingerprint, "%02X", md[i]);  // NOLINT
    fingerprint += 2;

    if (i < (n - 1))
      *fingerprint++ = ':';
    else
      *fingerprint++ = 0;
  }
}

void DtlsSocket::handleTimeout() {
  (void)BIO_reset(in_bio_);
  (void)BIO_reset(out_bio_);
  if (DTLSv1_handle_timeout(ssl_) > 0) {
    ELOG_DEBUG("Dtls timeout occurred!");

    // See what was written
    unsigned char* outBioData;
    int outBioLen = BIO_get_mem_data(out_bio_, &outBioData);
    if (outBioLen > DTLS_MTU) {
      ELOG_WARN("message: BIO data bigger than MTU - packet could be lost, outBioLen %u, MTU %u",
                outBioLen, DTLS_MTU);
    }

    // If out_bio_ is now nonzero-length, then we need to write the
    // data to the network. TODO(pedro): warning, MTU issues!
    if (outBioLen) {
      dtls_socket_ctx_->write(outBioData, outBioLen);
    }
  }
}

// TODO(pedro): assert(0) into exception, as elsewhere
void DtlsSocket::createSrtpSessionPolicies(srtp_policy_t& outboundPolicy,
                                           srtp_policy_t& inboundPolicy) {
  assert(is_handshake_completted);

  /* we assume that the default profile is in effect, for now */
  srtp_profile_t profile = srtp_profile_aes128_cm_sha1_80;
  int key_len = srtp_profile_get_master_key_length(profile);
  int salt_len = srtp_profile_get_master_salt_length(profile);

  /* get keys from srtp_key and initialize the inbound and outbound sessions */
  uint8_t* client_master_key_and_salt = new uint8_t[SRTP_MAX_KEY_LEN];
  uint8_t* server_master_key_and_salt = new uint8_t[SRTP_MAX_KEY_LEN];
  srtp_policy_t client_policy;
  memset(&client_policy, 0, sizeof(srtp_policy_t));
  client_policy.window_size = 128;
  client_policy.allow_repeat_tx = 1;
  srtp_policy_t server_policy;
  memset(&server_policy, 0, sizeof(srtp_policy_t));
  server_policy.window_size = 128;
  server_policy.allow_repeat_tx = 1;

  SrtpSessionKeys* srtp_key = getSrtpSessionKeys();
  /* set client_write key */
  client_policy.key = client_master_key_and_salt;
  if (srtp_key->clientMasterKeyLen != key_len) {
    ELOG_WARN("error: unexpected client key length");
    assert(0);
  }
  if (srtp_key->clientMasterSaltLen != salt_len) {
    ELOG_WARN("error: unexpected client salt length");
    assert(0);
  }

  memcpy(client_master_key_and_salt, srtp_key->clientMasterKey, key_len);
  memcpy(client_master_key_and_salt + key_len, srtp_key->clientMasterSalt, salt_len);

  /* initialize client SRTP policy from profile  */
  srtp_err_status_t err = srtp_crypto_policy_set_from_profile_for_rtp(&client_policy.rtp, profile);
  if (err) assert(0);

  err = srtp_crypto_policy_set_from_profile_for_rtcp(&client_policy.rtcp, profile);
  if (err) assert(0);
  client_policy.next = NULL;

  /* set server_write key */
  server_policy.key = server_master_key_and_salt;

  if (srtp_key->serverMasterKeyLen != key_len) {
    ELOG_WARN("error: unexpected server key length");
    assert(0);
  }
  if (srtp_key->serverMasterSaltLen != salt_len) {
    ELOG_WARN("error: unexpected salt length");
    assert(0);
  }

  memcpy(server_master_key_and_salt, srtp_key->serverMasterKey, key_len);
  memcpy(server_master_key_and_salt + key_len, srtp_key->serverMasterSalt, salt_len);

  delete srtp_key;

  /* initialize server SRTP policy from profile  */
  err = srtp_crypto_policy_set_from_profile_for_rtp(&server_policy.rtp, profile);
  if (err) assert(0);

  err = srtp_crypto_policy_set_from_profile_for_rtcp(&server_policy.rtcp, profile);
  if (err) assert(0);
  server_policy.next = NULL;

  if (socket_type_ == Client) {
    client_policy.ssrc.type = ssrc_any_outbound;
    outboundPolicy = client_policy;

    server_policy.ssrc.type = ssrc_any_inbound;
    inboundPolicy = server_policy;
  } else {
    server_policy.ssrc.type = ssrc_any_outbound;
    outboundPolicy = server_policy;

    client_policy.ssrc.type = ssrc_any_inbound;
    inboundPolicy = client_policy;
  }
  /* zeroize the input keys (but not the srtp session keys that are in use) */
  // not done...not much of a security whole imho...the lifetime of these seems odd though
  //    memset(client_master_key_and_salt, 0x00, SRTP_MAX_KEY_LEN);
  //    memset(server_master_key_and_salt, 0x00, SRTP_MAX_KEY_LEN);
  //    memset(&srtp_key, 0x00, sizeof(srtp_key));
}
