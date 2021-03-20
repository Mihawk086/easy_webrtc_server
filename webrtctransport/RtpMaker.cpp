#include "RtpMaker.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdlib>
#include <cstring>

#define RTP_HEADER_SIZE 12
#define MAX_RTP_PAYLOAD_SIZE 1400  // 1460  1500-20-12-8
#define RTP_VERSION 2
#define RTP_TCP_HEAD_SIZE 4

typedef struct rtp_header {
  uint16_t csrccount : 4;
  uint16_t extension : 1;
  uint16_t padding : 1;
  uint16_t version : 2;
  uint16_t type : 7;
  uint16_t markerbit : 1;
  uint16_t seq_number;
  uint32_t timestamp;
  uint32_t ssrc;
  uint32_t csrc[16];
} rtp_header;

static void set_buf_rtp_video_header(char* pbuffer, uint32_t dwssrc, uint32_t dwtimestample,
                                     uint16_t dwseqnum, bool marker) {
  rtp_header* rtp_hdr = (rtp_header*)pbuffer;
  memset(rtp_hdr, 0, RTP_HEADER_SIZE);
  rtp_hdr->type = 96;
  rtp_hdr->version = 2;
  rtp_hdr->ssrc = htonl(dwssrc);
  rtp_hdr->timestamp = htonl(dwtimestample);
  rtp_hdr->markerbit = marker;
  rtp_hdr->seq_number = htons(dwseqnum);
}

RtpMaker::RtpMaker() {}

RtpMaker::~RtpMaker() {}

void RtpMaker::InputH264Frame(char* buf, int len, uint32_t timestamp) {
  char* frameBuf = buf;
  int frameSize = len;

  if (frameSize <= MAX_RTP_PAYLOAD_SIZE) {
    memcpy(m_buf + RTP_HEADER_SIZE, frameBuf, frameSize);
    int rtpsize = frameSize + RTP_HEADER_SIZE;
    {
      set_buf_rtp_video_header(m_buf, m_ssrc, timestamp, m_seq++, true);
      if (m_RtpCallBack) {
        m_RtpCallBack(m_buf, rtpsize);
      }
    }
  } else {
    char FU_A[2] = {0};

    // �ְ��ο�live555
    FU_A[0] = (frameBuf[0] & 0xE0) | 28;
    FU_A[1] = 0x80 | (frameBuf[0] & 0x1f);

    frameBuf += 1;
    frameSize -= 1;

    while (frameSize + 2 > MAX_RTP_PAYLOAD_SIZE) {
      int rtpsize = RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;

      m_buf[RTP_HEADER_SIZE] = FU_A[0];
      m_buf[RTP_HEADER_SIZE + 1] = FU_A[1];
      memcpy(m_buf + RTP_HEADER_SIZE + 2, frameBuf, MAX_RTP_PAYLOAD_SIZE - 2);
      {
        set_buf_rtp_video_header(m_buf, m_ssrc, timestamp, m_seq++, false);
        if (m_RtpCallBack) {
          m_RtpCallBack(m_buf, rtpsize);
        }
      }
      frameBuf += MAX_RTP_PAYLOAD_SIZE - 2;
      frameSize -= MAX_RTP_PAYLOAD_SIZE - 2;
      FU_A[1] &= ~0x80;
    }

    {
      int rtpsize = +RTP_HEADER_SIZE + 2 + frameSize;

      FU_A[1] |= 0x40;
      m_buf[RTP_HEADER_SIZE] = FU_A[0];
      m_buf[RTP_HEADER_SIZE + 1] = FU_A[1];
      memcpy(m_buf + RTP_HEADER_SIZE + 2, frameBuf, frameSize);
      {
        set_buf_rtp_video_header(m_buf, m_ssrc, timestamp, m_seq++, true);
        if (m_RtpCallBack) {
          m_RtpCallBack(m_buf, rtpsize);
        }
      }
    }
  }
}
