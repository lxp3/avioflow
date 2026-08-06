#pragma once

#include "ffmpeg-common.h"
#include "metadata.h"
#include <vector>

namespace avioflow
{
  /**
   * @brief Stateful planar-float resampler built on libswresample.
   *
   * Holds swresample filter state across calls, so consecutive chunks join
   * without discontinuities at the boundaries. Input and output are planar
   * float: samples[channel][sample], matching AV_SAMPLE_FMT_FLTP so no
   * interleaving is needed.
   */
  class SingleStreamResampler
  {
  public:
    explicit SingleStreamResampler(const AudioResampleOptions &options);
    ~SingleStreamResampler() = default;

    SingleStreamResampler(const SingleStreamResampler &) = delete;
    SingleStreamResampler &operator=(const SingleStreamResampler &) = delete;
    SingleStreamResampler(SingleStreamResampler &&) = default;
    SingleStreamResampler &operator=(SingleStreamResampler &&) = default;

    // Convert one chunk. May return fewer samples than the ratio suggests,
    // since swresample buffers samples internally.
    std::vector<std::vector<float>> process(
        const std::vector<std::vector<float>> &samples);

    // Drain buffered samples. Call once at end of stream.
    std::vector<std::vector<float>> flush();

    int output_sample_rate() const { return options_.output_sample_rate; }
    int output_num_channels() const { return out_channels_; }

  private:
    void ensure_initialized(int input_channels);
    std::vector<std::vector<float>> convert(const uint8_t *const *in_data,
                                            int in_samples);

    AudioResampleOptions options_;
    SwrContextPtr swr_ctx_;
    int in_channels_ = 0;
    int out_channels_ = 0;
  };

} // namespace avioflow
