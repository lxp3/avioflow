
#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

namespace avioflow {

struct AVIOOpaqueContext {
  virtual ~AVIOOpaqueContext() = default;
};

// Global FFmpeg log control
void internal_set_log_level(const char *level);

// Capability discovery
std::vector<std::string> internal_get_supported_decoders();
std::vector<std::string> internal_get_supported_encoders();
std::vector<std::string> internal_get_supported_input_formats();
std::vector<std::string> internal_get_supported_output_formats();

// Simple macro/function for FFmpeg error checking
inline void check_av_error(int err, const std::string &msg) {
  if (err < 0) {
    char err_buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(err, err_buf, sizeof(err_buf));
    throw std::runtime_error(msg + ": " + err_buf);
  }
}

// RAII Deleters for FFmpeg structures
struct AVFormatContextDeleter {
  void operator()(AVFormatContext *p) {
    if (!p) {
      return;
    }
    AVIOContext *custom_io =
        (p->flags & AVFMT_FLAG_CUSTOM_IO) != 0 ? p->pb : nullptr;
    AVIOOpaqueContext *opaque = custom_io
                                    ? static_cast<AVIOOpaqueContext *>(custom_io->opaque)
                                    : nullptr;
    avformat_close_input(&p);
    if (custom_io) {
      av_freep(&custom_io->buffer);
      avio_context_free(&custom_io);
      delete opaque;
    }
  }
};
struct AVCodecContextDeleter {
  void operator()(AVCodecContext *p) { avcodec_free_context(&p); }
};
struct AVIOContextDeleter {
  void operator()(AVIOContext *p) {
    if (p) {
      av_freep(&p->buffer);
      avio_context_free(&p);
    }
  }
};
struct AVPacketDeleter {
  void operator()(AVPacket *p) { av_packet_free(&p); }
};
struct AVFrameDeleter {
  void operator()(AVFrame *p) { av_frame_free(&p); }
};
struct SwrContextDeleter {
  void operator()(SwrContext *p) { swr_free(&p); }
};

using AVFormatContextPtr =
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecContextPtr =
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVIOContextPtr = std::unique_ptr<AVIOContext, AVIOContextDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

// AVIO callback function types (raw C function pointers for FFmpeg)
using AVIOReadFunction = int (*)(void *, uint8_t *, int);
using AVIOWriteFunction = int (*)(void *, const uint8_t *, int);
using AVIOSeekFunction = int64_t (*)(void *, int64_t, int);

// AVIO read callback for streaming input
// Returns: >0 (bytes read), 0 (EOF), <0 (no data available, try again)
using AVIOReadCallback = std::function<int(uint8_t *, int)>;

} // namespace avioflow
