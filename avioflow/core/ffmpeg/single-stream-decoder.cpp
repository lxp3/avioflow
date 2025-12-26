
#include "single-stream-decoder.h"
#include "avio-context-handler.h"
#include "device-handler.h"
#include <iostream>

namespace avioflow {

SingleStreamDecoder::SingleStreamDecoder()
    : packet_(av_packet_alloc()), frame_(av_frame_alloc()) {}

void SingleStreamDecoder::open(const std::string &source) {
  if (source.find("audio=") == 0 || source.find("video=") == 0) {
    fmt_ctx_.reset(DeviceHandler::open_device(source));
  } else {
    fmt_ctx_.reset(AvioContextHandler::open_url(source));
  }
  setup_decoder();
}

void SingleStreamDecoder::open(const uint8_t *data, size_t size) {
  fmt_ctx_.reset(AvioContextHandler::open_memory(data, size));
  setup_decoder();
}

void SingleStreamDecoder::setup_decoder() {
  check_av_error(avformat_find_stream_info(fmt_ctx_.get(), nullptr),
                 "Could not find stream info");

  audio_stream_index_ = av_find_best_stream(fmt_ctx_.get(), AVMEDIA_TYPE_AUDIO,
                                            -1, -1, nullptr, 0);
  if (audio_stream_index_ < 0)
    throw std::runtime_error("Could not find audio stream");

  AVStream *stream = fmt_ctx_->streams[audio_stream_index_];
  const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!codec)
    throw std::runtime_error("Could not find decoder");

  codec_ctx_.reset(avcodec_alloc_context3(codec));
  check_av_error(
      avcodec_parameters_to_context(codec_ctx_.get(), stream->codecpar),
      "Could not copy codec params");
  check_av_error(avcodec_open2(codec_ctx_.get(), codec, nullptr),
                 "Could not open codec");

  // Populate metadata
  metadata_.sample_rate = codec_ctx_->sample_rate;
  metadata_.num_channels = codec_ctx_->ch_layout.nb_channels;
  metadata_.duration = static_cast<double>(fmt_ctx_->duration) / AV_TIME_BASE;
  metadata_.format = fmt_ctx_->iformat->name;

  // Initialize output buffers
  output_buffer_.resize(metadata_.num_channels);
  output_ptrs_.resize(metadata_.num_channels);
  output_spans_.resize(metadata_.num_channels);

  eof_reached_ = false;
  setup_resampler();
}

void SingleStreamDecoder::setup_resampler() {
  // Check if resampling is actually needed
  if (codec_ctx_->sample_fmt == TARGET_FORMAT) {
    needs_resample_ = false;
    return;
  }

  needs_resample_ = true;
  SwrContext *swr = nullptr;
  check_av_error(swr_alloc_set_opts2(&swr, &codec_ctx_->ch_layout,
                                     TARGET_FORMAT, codec_ctx_->sample_rate,
                                     &codec_ctx_->ch_layout,
                                     codec_ctx_->sample_fmt,
                                     codec_ctx_->sample_rate, 0, nullptr),
                 "Could not initialize resampler");

  swr_ctx_.reset(swr);
  check_av_error(swr_init(swr_ctx_.get()),
                 "Could not initialize resampler context");
}

void SingleStreamDecoder::ensure_buffer_capacity(int num_samples) {
  if (num_samples <= max_samples_per_frame_)
    return;

  // Reserve extra capacity to avoid frequent reallocations
  max_samples_per_frame_ = num_samples + (num_samples / 4); // 25% extra

  for (int i = 0; i < metadata_.num_channels; ++i) {
    output_buffer_[i].resize(max_samples_per_frame_);
    output_ptrs_[i] = reinterpret_cast<uint8_t *>(output_buffer_[i].data());
  }
}

bool SingleStreamDecoder::decode_frame_internal() {
  while (av_read_frame(fmt_ctx_.get(), packet_.get()) >= 0) {
    if (packet_->stream_index == audio_stream_index_) {
      int ret = avcodec_send_packet(codec_ctx_.get(), packet_.get());
      av_packet_unref(packet_.get());
      if (ret < 0)
        continue;

      ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        av_frame_unref(frame_.get());
        continue;
      } else if (ret < 0) {
        av_frame_unref(frame_.get());
        throw std::runtime_error("Error during decoding");
      }

      int num_samples = frame_->nb_samples;
      ensure_buffer_capacity(num_samples);

      if (needs_resample_) {
        // Use swr_convert to write directly into pre-allocated buffer
        int converted = swr_convert(
            swr_ctx_.get(), output_ptrs_.data(), num_samples,
            const_cast<const uint8_t **>(frame_->extended_data), num_samples);

        if (converted < 0) {
          av_frame_unref(frame_.get());
          throw std::runtime_error("Error during resampling");
        }
        last_decoded_samples_ = converted;
      } else {
        // Zero-copy path: format already matches, copy directly from frame
        for (int c = 0; c < metadata_.num_channels; ++c) {
          const float *src =
              reinterpret_cast<const float *>(frame_->extended_data[c]);
          std::copy(src, src + num_samples, output_buffer_[c].data());
        }
        last_decoded_samples_ = num_samples;
      }

      // Update spans to reflect actual decoded samples
      for (int c = 0; c < metadata_.num_channels; ++c) {
        output_spans_[c] =
            std::span<const float>(output_buffer_[c].data(),
                                   static_cast<size_t>(last_decoded_samples_));
      }

      av_frame_unref(frame_.get());
      return true;
    }
    av_packet_unref(packet_.get());
  }

  eof_reached_ = true;
  return false;
}

std::span<std::span<const float>> SingleStreamDecoder::decode_next_view() {
  if (!decode_frame_internal()) {
    return {}; // EOF
  }
  return std::span<std::span<const float>>(output_spans_);
}

std::vector<std::vector<float>> SingleStreamDecoder::decode_next() {
  if (!decode_frame_internal()) {
    return {}; // EOF
  }

  // Copy from internal buffer to output vector
  std::vector<std::vector<float>> result(metadata_.num_channels);
  for (int c = 0; c < metadata_.num_channels; ++c) {
    result[c].assign(output_buffer_[c].begin(),
                     output_buffer_[c].begin() + last_decoded_samples_);
  }
  return result;
}

std::vector<std::vector<float>> SingleStreamDecoder::get_all_samples() {
  std::vector<std::vector<float>> all_samples(metadata_.num_channels);

  // Pre-allocate based on estimated duration (with 10% buffer)
  if (metadata_.duration > 0) {
    size_t estimated_samples =
        static_cast<size_t>(metadata_.duration * metadata_.sample_rate * 1.1);
    for (auto &channel : all_samples) {
      channel.reserve(estimated_samples);
    }
  }

  // Seek to start for safety
  avformat_seek_file(fmt_ctx_.get(), audio_stream_index_, 0, 0, 0, 0);
  avcodec_flush_buffers(codec_ctx_.get());
  eof_reached_ = false;

  // Use zero-copy view for efficient reading
  while (true) {
    auto frame_view = decode_next_view();
    if (frame_view.empty())
      break;

    for (int c = 0; c < metadata_.num_channels; ++c) {
      all_samples[c].insert(all_samples[c].end(), frame_view[c].begin(),
                            frame_view[c].end());
    }
  }

  return all_samples;
}

} // namespace avioflow
