#pragma once

#include "ffmpeg-common.h"
#include "metadata.h"
#include <string>
#include <vector>

namespace avioflow {

struct EncoderMemoryOutput {
  std::vector<uint8_t> data;
  size_t position = 0;
};

class SingleStreamEncoder {
public:
  explicit SingleStreamEncoder(const AudioWriteOptions &options = {});
  ~SingleStreamEncoder();

  SingleStreamEncoder(const SingleStreamEncoder &) = delete;
  SingleStreamEncoder &operator=(const SingleStreamEncoder &) = delete;

  SingleStreamEncoder(SingleStreamEncoder &&) = delete;
  SingleStreamEncoder &operator=(SingleStreamEncoder &&) = delete;

  void save(const std::string &path, const std::vector<std::vector<float>> &samples);
  std::vector<uint8_t> save_buffer(const std::vector<std::vector<float>> &samples);

private:
  void reset();
  void validate_input(const std::vector<std::vector<float>> &samples,
                      const std::string &path) const;
  void setup_output(const std::string &path,
                    const std::vector<std::vector<float>> &samples);
  void setup_resampler();
  void encode_all_samples(const std::vector<std::vector<float>> &samples);
  void encode_chunk(const std::vector<std::vector<float>> &samples,
                    int start_sample,
                    int num_samples);
  void flush_encoder();
  void write_packets();

  AudioWriteOptions options_;
  AVFormatContext *fmt_ctx_ = nullptr;
  AVStream *stream_ = nullptr;
  const AVCodec *codec_ = nullptr;
  AVCodecContextPtr codec_ctx_;
  SwrContextPtr swr_ctx_;
  AVPacketPtr packet_;
  AVFramePtr src_frame_;
  AVFramePtr enc_frame_;
  int64_t next_pts_ = 0;
  int input_channels_ = 0;
  int total_input_samples_ = 0;
  EncoderMemoryOutput *memory_output_ = nullptr;
};

} // namespace avioflow
