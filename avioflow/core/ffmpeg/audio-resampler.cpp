#include "audio-resampler.h"
#include <stdexcept>

namespace avioflow
{
  SingleStreamResampler::SingleStreamResampler(const AudioResampleOptions &options)
      : options_(options)
  {
    if (options_.input_sample_rate <= 0)
      throw std::invalid_argument("input_sample_rate must be greater than zero");
    if (options_.output_sample_rate <= 0)
      throw std::invalid_argument("output_sample_rate must be greater than zero");
    if (options_.output_num_channels && *options_.output_num_channels <= 0)
      throw std::invalid_argument("output_num_channels must be greater than zero");
  }

  void SingleStreamResampler::ensure_initialized(int input_channels)
  {
    if (swr_ctx_)
    {
      if (input_channels != in_channels_)
        throw std::invalid_argument(
            "channel count changed between resampler calls");
      return;
    }

    if (input_channels <= 0)
      throw std::invalid_argument("input must have at least one channel");

    in_channels_ = input_channels;
    out_channels_ = options_.output_num_channels.value_or(input_channels);

    AVChannelLayout in_layout{};
    AVChannelLayout out_layout{};
    av_channel_layout_default(&in_layout, in_channels_);
    av_channel_layout_default(&out_layout, out_channels_);

    SwrContext *swr = nullptr;
    const int err = swr_alloc_set_opts2(
        &swr, &out_layout, AV_SAMPLE_FMT_FLTP, options_.output_sample_rate,
        &in_layout, AV_SAMPLE_FMT_FLTP, options_.input_sample_rate, 0, nullptr);

    av_channel_layout_uninit(&in_layout);
    av_channel_layout_uninit(&out_layout);
    check_av_error(err, "Could not allocate resampler context");

    swr_ctx_.reset(swr);
    check_av_error(swr_init(swr_ctx_.get()),
                   "Could not initialize resampler context");
  }

  std::vector<std::vector<float>> SingleStreamResampler::convert(
      const uint8_t *const *in_data, int in_samples)
  {
    // Account for samples still buffered inside swresample, otherwise the
    // output buffer overflows or the tail gets truncated.
    const int64_t delay = swr_get_delay(swr_ctx_.get(), options_.input_sample_rate);
    const int capacity = static_cast<int>(av_rescale_rnd(
        delay + in_samples, options_.output_sample_rate,
        options_.input_sample_rate, AV_ROUND_UP));

    std::vector<std::vector<float>> out(static_cast<size_t>(out_channels_));
    if (capacity <= 0)
      return out;

    for (auto &channel : out)
      channel.resize(static_cast<size_t>(capacity));

    std::vector<uint8_t *> out_ptrs(static_cast<size_t>(out_channels_));
    for (int c = 0; c < out_channels_; ++c)
      out_ptrs[static_cast<size_t>(c)] =
          reinterpret_cast<uint8_t *>(out[static_cast<size_t>(c)].data());

    const int converted = swr_convert(swr_ctx_.get(), out_ptrs.data(), capacity,
                                      in_data, in_samples);
    check_av_error(converted, "Error during resampling");

    for (auto &channel : out)
      channel.resize(static_cast<size_t>(converted));
    return out;
  }

  std::vector<std::vector<float>> SingleStreamResampler::process(
      const std::vector<std::vector<float>> &samples)
  {
    if (samples.empty())
      return {};

    const size_t num_samples = samples[0].size();
    for (const auto &channel : samples)
    {
      if (channel.size() != num_samples)
        throw std::invalid_argument("all channels must have the same length");
    }

    ensure_initialized(static_cast<int>(samples.size()));

    // Planar float maps directly onto AV_SAMPLE_FMT_FLTP, so channel pointers
    // can be handed to swresample without interleaving.
    std::vector<const uint8_t *> in_ptrs(samples.size());
    for (size_t c = 0; c < samples.size(); ++c)
      in_ptrs[c] = reinterpret_cast<const uint8_t *>(samples[c].data());

    return convert(in_ptrs.data(), static_cast<int>(num_samples));
  }

  std::vector<std::vector<float>> SingleStreamResampler::flush()
  {
    if (!swr_ctx_)
      return {};
    return convert(nullptr, 0);
  }

} // namespace avioflow
