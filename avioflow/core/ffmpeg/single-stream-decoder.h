
#pragma once

#include "ffmpeg-common.h"
#include "metadata.h"
#include <span>
#include <string>
#include <vector>

namespace avioflow {

class SingleStreamDecoder {
public:
  SingleStreamDecoder();
  ~SingleStreamDecoder() = default;

  // Direct opening - accepts file path, URL, or device (e.g.,
  // "audio=Microphone")
  void open(const std::string &source);

  // Open from memory buffer (e.g., raw encoded audio data with header)
  void open(const uint8_t *data, size_t size);

  // Decoding - returns samples per channel (copies data)
  std::vector<std::vector<float>> decode_next();
  std::vector<std::vector<float>> get_all_samples();

  // Zero-copy decoding - returns views into internal buffer
  // WARNING: Data is only valid until the next decode call
  std::span<std::span<const float>> decode_next_view();

  // Check if there are more frames to decode
  bool has_more() const { return !eof_reached_; }

  const Metadata &get_metadata() const { return metadata_; }

private:
  void setup_decoder();
  void setup_resampler();
  void ensure_buffer_capacity(int num_samples);
  bool decode_frame_internal();

  // Core FFmpeg contexts
  AVFormatContextPtr fmt_ctx_;
  AVCodecContextPtr codec_ctx_;
  SwrContextPtr swr_ctx_;

  AVPacketPtr packet_;
  AVFramePtr frame_;

  Metadata metadata_;
  int audio_stream_index_ = -1;
  bool eof_reached_ = false;
  bool needs_resample_ = true;
  int last_decoded_samples_ = 0;

  // Pre-allocated output buffers for efficient decoding
  std::vector<std::vector<float>> output_buffer_;
  std::vector<uint8_t *> output_ptrs_;
  std::vector<std::span<const float>> output_spans_;
  int max_samples_per_frame_ = 0;

  // Target output format
  static constexpr AVSampleFormat TARGET_FORMAT = AV_SAMPLE_FMT_FLTP;
};

} // namespace avioflow
