#include "rtp_maker.h"

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
  char* frame_buf = buf;
  int frame_size = len;

  if (frame_size <= MAX_RTP_PAYLOAD_SIZE) {
    memcpy(buf_ + RTP_HEADER_SIZE, frame_buf, frame_size);
    int rtpsize = frame_size + RTP_HEADER_SIZE;
    {
      set_buf_rtp_video_header(buf_, ssrc_, timestamp, seq_++, true);
      if (make_rtp_completed_callback) {
        make_rtp_completed_callback(buf_, rtpsize);
      }
    }
  } else {
    char FU_A[2] = {0};

    FU_A[0] = (frame_buf[0] & 0xE0) | 28;
    FU_A[1] = 0x80 | (frame_buf[0] & 0x1f);

    frame_buf += 1;
    frame_size -= 1;

    while (frame_size + 2 > MAX_RTP_PAYLOAD_SIZE) {
      int rtpsize = RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;

      buf_[RTP_HEADER_SIZE] = FU_A[0];
      buf_[RTP_HEADER_SIZE + 1] = FU_A[1];
      memcpy(buf_ + RTP_HEADER_SIZE + 2, frame_buf, MAX_RTP_PAYLOAD_SIZE - 2);
      {
        set_buf_rtp_video_header(buf_, ssrc_, timestamp, seq_++, false);
        if (make_rtp_completed_callback) {
          make_rtp_completed_callback(buf_, rtpsize);
        }
      }
      frame_buf += MAX_RTP_PAYLOAD_SIZE - 2;
      frame_size -= MAX_RTP_PAYLOAD_SIZE - 2;
      FU_A[1] &= ~0x80;
    }

    {
      int rtpsize = +RTP_HEADER_SIZE + 2 + frame_size;

      FU_A[1] |= 0x40;
      buf_[RTP_HEADER_SIZE] = FU_A[0];
      buf_[RTP_HEADER_SIZE + 1] = FU_A[1];
      memcpy(buf_ + RTP_HEADER_SIZE + 2, frame_buf, frame_size);
      {
        set_buf_rtp_video_header(buf_, ssrc_, timestamp, seq_++, true);
        if (make_rtp_completed_callback) {
          make_rtp_completed_callback(buf_, rtpsize);
        }
      }
    }
  }
}
