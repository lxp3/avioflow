
#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <functional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libavdevice/avdevice.h>
}

namespace avioflow {

// Simple macro/function for FFmpeg error checking
inline void check_av_error(int err, const std::string& msg) {
    if (err < 0) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(err, err_buf, sizeof(err_buf));
        throw std::runtime_error(msg + ": " + err_buf);
    }
}

// RAII Deleters for FFmpeg structures
struct AVFormatContextDeleter { void operator()(AVFormatContext* p) { avformat_close_input(&p); } };
struct AVCodecContextDeleter { void operator()(AVCodecContext* p) { avcodec_free_context(&p); } };
struct AVIOContextDeleter { void operator()(AVIOContext* p) { if (p) { av_freep(&p->buffer); avio_context_free(&p); } } };
struct AVPacketDeleter { void operator()(AVPacket* p) { av_packet_free(&p); } };
struct AVFrameDeleter { void operator()(AVFrame* p) { av_frame_free(&p); } };
struct SwrContextDeleter { void operator()(SwrContext* p) { swr_free(&p); } };

using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVIOContextPtr = std::unique_ptr<AVIOContext, AVIOContextDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

// AVIO callback function types (raw C function pointers for FFmpeg)
using AVIOReadFunction = int (*)(void*, uint8_t*, int);
using AVIOWriteFunction = int (*)(void*, const uint8_t*, int);
using AVIOSeekFunction = int64_t (*)(void*, int64_t, int);

// AVIO read callback for streaming input
// Returns: >0 (bytes read), 0 (EOF), <0 (no data available, try again)
using AVIOReadCallback = std::function<int(uint8_t*, int)>;

} // namespace avioflow
