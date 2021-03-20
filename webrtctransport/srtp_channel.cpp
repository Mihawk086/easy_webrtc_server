/*
 * Srtpchannel.cpp
 */

#include "srtp_channel.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <srtp2/srtp.h>

#include "string.h"

#include <mutex>
#include <string>

namespace erizo {

bool SrtpChannel::initialized = false;
std::mutex SrtpChannel::sessionMutex_;

constexpr int kKeyStringLength = 32;

uint8_t nibble_to_hex_char(uint8_t nibble) {
  char buf[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

  return buf[nibble & 0xF];
}

std::string octet_string_hex_string(const void *s, int length) {
  if (length != 16) {
    return "";
  }

  const uint8_t *str = (const uint8_t *)s;
  int i = 0;

  char bit_string[kKeyStringLength * 2];

  for (i = 0; i < kKeyStringLength * 2; i += 2) {
    bit_string[i] = nibble_to_hex_char(*str >> 4);
    bit_string[i + 1] = nibble_to_hex_char(*str++ & 0xF);
  }
  return std::string(bit_string);
}

SrtpChannel::SrtpChannel() {
  std::lock_guard<std::mutex> lock(SrtpChannel::sessionMutex_);
  if (SrtpChannel::initialized != true) {
    int res = srtp_init();
    ELOG_DEBUG("Initialized SRTP library %d", res);
    SrtpChannel::initialized = true;
  }

  active_ = false;
  send_session_ = NULL;
  receive_session_ = NULL;
}

SrtpChannel::~SrtpChannel() {
  active_ = false;
  if (send_session_ != NULL) {
    srtp_dealloc(send_session_);
    send_session_ = NULL;
  }
  if (receive_session_ != NULL) {
    srtp_dealloc(receive_session_);
    receive_session_ = NULL;
  }
}

bool SrtpChannel::setRtpParams(const std::string &sendingKey, const std::string &receivingKey) {
  ELOG_DEBUG("Configuring srtp local key %s remote key %s", sendingKey.c_str(),
             receivingKey.c_str());
  if (configureSrtpSession(&send_session_, sendingKey, SENDING) &&
      configureSrtpSession(&receive_session_, receivingKey, RECEIVING)) {
    active_ = true;
    return active_;
  }
  return false;
}

bool SrtpChannel::setRtcpParams(const std::string &sendingKey, const std::string &receivingKey) {
  return 0;
}

int SrtpChannel::protectRtp(char *buffer, int *len) {
  if (!active_) {
    return -1;
  }
  int val = srtp_protect(send_session_, buffer, len);
  if (val == 0) {
    return 0;
  } else {
    RtcpHeader *head = reinterpret_cast<RtcpHeader *>(buffer);
    RtpHeader *headrtp = reinterpret_cast<RtpHeader *>(buffer);

    if (val != 10) {  // Do not warn about reply errors
      ELOG_DEBUG("Error SrtpChannel::protectRtp %u packettype %d pt %d seqnum %u", val,
                 head->packettype, headrtp->payloadtype, headrtp->seqnum);
    }
    return -1;
  }
}

int SrtpChannel::unprotectRtp(char *buffer, int *len) {
  if (!active_) {
    return -1;
  }
  int val = srtp_unprotect(receive_session_, reinterpret_cast<char *>(buffer), len);
  if (val == 0) {
    return 0;
  } else {
    RtcpHeader *head = reinterpret_cast<RtcpHeader *>(buffer);
    RtpHeader *headrtp = reinterpret_cast<RtpHeader *>(buffer);
    if (val != 10) {  // Do not warn about reply errors
      ELOG_DEBUG("Error SrtpChannel::unprotectRtp %u packettype %d pt %d", val, head->packettype,
                 headrtp->payloadtype);
    }
    return -1;
  }
}

int SrtpChannel::protectRtcp(char *buffer, int *len) {
  if (!active_) {
    return -1;
  }
  int val = srtp_protect_rtcp(send_session_, reinterpret_cast<char *>(buffer), len);
  if (val == 0) {
    return 0;
  } else {
    RtcpHeader *head = reinterpret_cast<RtcpHeader *>(buffer);
    if (val != 10) {  // Do not warn about reply errors
      ELOG_DEBUG("Error SrtpChannel::protectRtcp %upackettype %d ", val, head->packettype);
    }
    return -1;
  }
}

int SrtpChannel::unprotectRtcp(char *buffer, int *len) {
  if (!active_) {
    return -1;
  }
  int val = srtp_unprotect_rtcp(receive_session_, buffer, len);
  if (val == 0) {
    return 0;
  } else {
    if (val != 10) {  // Do not warn about reply errors
      ELOG_DEBUG("Error SrtpChannel::unprotectRtcp %u", val);
    }
    return -1;
  }
}

static std::string base64Decode(char *input, int length, bool newLine) {
  BIO *b64 = NULL;
  BIO *bmem = NULL;
  char *buffer = (char *)malloc(length);
  memset(buffer, 0, length);
  b64 = BIO_new(BIO_f_base64());
  if (!newLine) {
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  }
  bmem = BIO_new_mem_buf(input, length);
  bmem = BIO_push(b64, bmem);
  BIO_read(bmem, buffer, length);
  BIO_free_all(bmem);

  std::string out(buffer);
  free(buffer);

  return out;
}

bool SrtpChannel::configureSrtpSession(srtp_t *session, const std::string &key,
                                       enum TransmissionType type) {
  srtp_policy_t policy;
  memset(&policy, 0, sizeof(policy));
  srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
  srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
  if (type == SENDING) {
    policy.ssrc.type = ssrc_any_outbound;
  } else {
    policy.ssrc.type = ssrc_any_inbound;
  }

  policy.ssrc.value = 0;
  policy.window_size = 1024;
  policy.allow_repeat_tx = 1;
  policy.next = NULL;
  ELOG_DEBUG("auth_tag_len %d", policy.rtp.auth_tag_len);

  /*
  gsize len = 0;
  uint8_t *akey = reinterpret_cast<uint8_t*>(g_base64_decode(reinterpret_cast<const
  gchar*>(key.c_str()), &len)); ELOG_DEBUG("set master key/salt to %s/",
  octet_string_hex_string(akey, 16).c_str());
  // allocate and initialize the SRTP session
  policy.key = akey;
  int res = srtp_create(session, &policy);
  if (res != 0) {
    ELOG_ERROR("Failed to create srtp session with %s, %d", octet_string_hex_string(akey,
  16).c_str(), res);
  }
  g_free(akey); akey = NULL;
  */

  std::string strkey = base64Decode(const_cast<char *>(key.c_str()), key.size(), false);
  policy.key = (unsigned char *)strkey.c_str();
  int res = srtp_create(session, &policy);
  if (res != 0) {
    ELOG_ERROR("Failed to create srtp session with %s, %d",
               octet_string_hex_string(strkey.c_str(), 16).c_str(), res);
  }

  return res != 0 ? false : true;
}

} /*namespace erizo */
