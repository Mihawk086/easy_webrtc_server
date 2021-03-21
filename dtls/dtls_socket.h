#ifndef ERIZO_SRC_ERIZO_DTLS_DTLSSOCKET_H_
#define ERIZO_SRC_ERIZO_DTLS_DTLSSOCKET_H_

extern "C" {
#include "srtp2/srtp.h"
}

#include <memory>
#include <mutex>
#include <string>

#include "log/logger.h"
#include "openssl/crypto.h"
#include "openssl/e_os2.h"
#include "openssl/err.h"
#include "openssl/rand.h"
#include "openssl/ssl.h"

const int SRTP_MASTER_KEY_KEY_LEN = 16;
const int SRTP_MASTER_KEY_SALT_LEN = 14;
static const int DTLS_MTU = 1472;

namespace dtls {
class DtlsSocketContext;

class SrtpSessionKeys {
 public:
  SrtpSessionKeys() {
    clientMasterKey = new unsigned char[SRTP_MASTER_KEY_KEY_LEN];
    clientMasterKeyLen = 0;
    clientMasterSalt = new unsigned char[SRTP_MASTER_KEY_SALT_LEN];
    clientMasterSaltLen = 0;
    serverMasterKey = new unsigned char[SRTP_MASTER_KEY_KEY_LEN];
    serverMasterKeyLen = 0;
    serverMasterSalt = new unsigned char[SRTP_MASTER_KEY_SALT_LEN];
    serverMasterSaltLen = 0;
  }
  ~SrtpSessionKeys() {
    if (clientMasterKey) {
      delete[] clientMasterKey;
      clientMasterKey = NULL;
    }
    if (serverMasterKey) {
      delete[] serverMasterKey;
      serverMasterKey = NULL;
    }
    if (clientMasterSalt) {
      delete[] clientMasterSalt;
      clientMasterSalt = NULL;
    }
    if (serverMasterSalt) {
      delete[] serverMasterSalt;
      serverMasterSalt = NULL;
    }
  }
  unsigned char *clientMasterKey;
  int clientMasterKeyLen;
  unsigned char *serverMasterKey;
  int serverMasterKeyLen;
  unsigned char *clientMasterSalt;
  int clientMasterSaltLen;
  unsigned char *serverMasterSalt;
  int serverMasterSaltLen;
};

class DtlsSocket {
 public:
  enum SocketType { Client, Server };
  // Creates an SSL socket, and if client sets state to connect_state and
  // if server sets state to accept_state.  Sets SSL BIO's.
  DtlsSocket(DtlsSocketContext *socketContext, enum SocketType type);
  ~DtlsSocket();

  void close();

  // Inspects packet to see if it's a DTLS packet, if so continue processing
  bool handlePacketMaybe(const unsigned char *bytes, unsigned int len);

  // Retrieves the finger print of the certificate presented by the remote party
  bool getRemoteFingerprint(char *fingerprint);

  // Retrieves the finger print of the certificate presented by the remote party and checks
  // it agains the passed in certificate
  bool checkFingerprint(const char *fingerprint, unsigned int len);

  // Retrieves the finger print of our local certificate, same as getMyCertFingerprint
  void getMyCertFingerprint(char *fingerprint);

  // For client sockets only - causes a client handshake to start (doHandshakeIteration)
  void startClient();

  // Retreives the SRTP session keys from the Dtls session
  SrtpSessionKeys *getSrtpSessionKeys();

  // Utility fn to compute a certificates fingerprint
  static void computeFingerprint(X509 *cert, char *fingerprint);

  // Retrieves the DTLS negotiated SRTP profile - may return 0 if profile selection failed
  SRTP_PROTECTION_PROFILE *getSrtpProfile();

  // Creates SRTP session policies appropriately based on socket type (client vs server) and keys
  // extracted from the DTLS handshake process
  void createSrtpSessionPolicies(srtp_policy_t &outboundPolicy,
                                 srtp_policy_t &inboundPolicy);  // NOLINT

  void handleTimeout();

 private:
  // Causes an immediate handshake iteration to happen, which will retransmit the handshake
  void forceRetransmit();

  // Give CPU cyces to the handshake process - checks current state and acts appropraitely
  void doHandshakeIteration();

  // Internals
  DtlsSocketContext *dtls_socket_ctx_;

  // OpenSSL context data
  SSL *ssl_;
  BIO *in_bio_;
  BIO *out_bio_;

  SocketType socket_type_;
  bool is_handshake_completted;
  std::mutex handshake_mutex_;
};

class DtlsReceiver {
 public:
  virtual void onDtlsPacket(DtlsSocketContext *ctx, const unsigned char *data,
                            unsigned int len) = 0;
  virtual void onHandshakeCompleted(DtlsSocketContext *ctx, std::string clientKey,
                                    std::string serverKey, std::string srtp_profile) = 0;
  virtual void onHandshakeFailed(DtlsSocketContext *ctx, const std::string &error) = 0;
};

class DtlsSocketContext {
 public:
  bool started;
  // memory is only valid for duration of callback; must be copied if queueing
  // is required
  DtlsSocketContext();
  virtual ~DtlsSocketContext();

  void close();

  void start();
  void read(const unsigned char *data, unsigned int len);
  void write(const unsigned char *data, unsigned int len);
  void handshakeCompleted();
  void handshakeFailed(const char *err);
  void setDtlsReceiver(DtlsReceiver *recv);
  void setDtlsSocket(DtlsSocket *sock) { dtls_socket_ = sock; }
  std::string getFingerprint() const;

  void handleTimeout();

  enum PacketType { rtp, dtls, stun, unknown };

  // Creates a new DtlsSocket to be used as a client
  DtlsSocket *createClient();

  // Creates a new DtlsSocket to be used as a server
  DtlsSocket *createServer();

  // Returns the fingerprint of the user cert that was passed into the constructor
  void getMyCertFingerprint(char *fingerprint);

  // The default SrtpProfile used at construction time (default is:
  // SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32)
  static const char *DefaultSrtpProfile;

  // Changes the default SRTP profiles supported (default is:
  // SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32)
  void setSrtpProfiles(const char *policyStr);

  // Changes the default DTLS Cipher Suites supported
  void setCipherSuites(const char *cipherSuites);

  SSL_CTX *getSSLContext();

  // Examines the first few bits of a packet to determine its type: rtp, dtls, stun or unknown
  static PacketType demuxPacket(const unsigned char *buf, unsigned int len);

  static X509 *mCert;
  static EVP_PKEY *privkey;

  static void Init();
  static void Destroy();

 protected:
  DtlsSocket *dtls_socket_;
  DtlsReceiver *dtls_recevier_;

 private:
  // Creates a DTLS SSL Context and enables srtp extension, also sets the private and public key
  // cert
  SSL_CTX *ssl_ctx_;
};
}  // namespace dtls

#endif  // ERIZO_SRC_ERIZO_DTLS_DTLSSOCKET_H_
