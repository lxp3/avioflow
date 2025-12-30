
#pragma once

#include "ffmpeg-common.h"
#include "metadata.h"
#include <optional>
#include <string>

namespace avioflow
{

  class SingleStreamDecoder
  {
  public:
    explicit SingleStreamDecoder(
        std::optional<int> output_sample_rate = std::nullopt,
        std::optional<int> output_num_channels = std::nullopt);
    ~SingleStreamDecoder() = default;

    // Direct opening - accepts file path, URL, or device (e.g.,
    // "audio=Microphone")
    void open(const std::string &source);

    // Open from memory buffer (e.g., raw encoded audio data with header)
    void open(const uint8_t *data, size_t size);

    // Open from memory buffer with raw PCM data (no header)
    void open(const uint8_t *data, size_t size, int sample_rate, int channels,
              const std::string &pcm_format);

    // Decode next frame - returns FrameOutput with pointer to internal buffer
    // WARNING: Data is only valid until the next decode call
    FrameOutput decode_next();

    // Check if there are more frames to decode
    bool has_more() const { return !eof_reached_; }

    const Metadata &get_metadata() const { return metadata_; }

  private:
    void setup_decoder();
    void setup_resampler(AVFrame *frame);

    // Core FFmpeg contexts
    AVFormatContextPtr fmt_ctx_;
    AVCodecContextPtr codec_ctx_;
    SwrContextPtr swr_ctx_;

    AVPacketPtr packet_;
    AVFramePtr frame_;
    AVFramePtr converted_frame_;

    Metadata metadata_;
    int audio_stream_index_ = -1;
    std::optional<int> output_sample_rate_;
    std::optional<int> output_num_channels_;
    bool eof_reached_ = false;
    bool needs_resample_ = true;
    bool resampler_initialized_ = false;

    // Actual output parameters (resolved after first frame)
    int out_sample_rate_ = 0;
    int out_num_channels_ = 0;

    // static
    static constexpr AVSampleFormat output_sample_format_ = AV_SAMPLE_FMT_FLTP;
  };

} // namespace avioflow
