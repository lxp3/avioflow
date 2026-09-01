#include "single-stream-encoder.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace avioflow {

namespace {
int write_memory(void *opaque, const uint8_t *data, int size) {
  auto *out = static_cast<std::vector<uint8_t> *>(opaque);
  out->insert(out->end(), data, data + size);
  return size;
}
int64_t seek_memory(void *opaque, int64_t offset, int whence) {
  auto *out = static_cast<std::vector<uint8_t> *>(opaque);
  if (whence == AVSEEK_SIZE) return static_cast<int64_t>(out->size());
  if ((whence & ~AVSEEK_FORCE) != SEEK_SET) return AVERROR(EINVAL);
  if (offset < 0) return AVERROR(EINVAL);
  auto pos = static_cast<size_t>(offset);
  if (pos > out->size()) out->resize(pos);
  return offset;
}

const void *get_supported_config(const AVCodec *codec,
                                 AVCodecConfig config,
                                 int *num_configs = nullptr) {
  const void *configs = nullptr;
  check_av_error(avcodec_get_supported_config(nullptr, codec, config, 0, &configs, num_configs),
                 "Could not query encoder supported config");
  return configs;
}

AVSampleFormat parse_sample_format(const std::optional<std::string> &sample_format) {
  if (!sample_format.has_value()) {
    return AV_SAMPLE_FMT_NONE;
  }
  AVSampleFormat fmt = av_get_sample_fmt(sample_format->c_str());
  if (fmt == AV_SAMPLE_FMT_NONE) {
    throw std::runtime_error("Unsupported sample format: " + *sample_format);
  }
  return fmt;
}

bool codec_supports_sample_format(const AVCodec *codec, AVSampleFormat sample_fmt) {
  const auto *sample_fmts = static_cast<const AVSampleFormat *>(
      get_supported_config(codec, AV_CODEC_CONFIG_SAMPLE_FORMAT));
  if (!sample_fmts) {
    return true;
  }
  for (const AVSampleFormat *fmt = sample_fmts; *fmt != AV_SAMPLE_FMT_NONE; ++fmt) {
    if (*fmt == sample_fmt) {
      return true;
    }
  }
  return false;
}

AVSampleFormat choose_encoder_sample_format(const AVCodec *codec,
                                            const std::optional<std::string> &requested) {
  AVSampleFormat requested_fmt = parse_sample_format(requested);
  if (requested_fmt != AV_SAMPLE_FMT_NONE) {
    if (!codec_supports_sample_format(codec, requested_fmt)) {
      throw std::runtime_error("Encoder does not support requested sample format");
    }
    return requested_fmt;
  }

  const auto *sample_fmts = static_cast<const AVSampleFormat *>(
      get_supported_config(codec, AV_CODEC_CONFIG_SAMPLE_FORMAT));
  if (sample_fmts) {
    return sample_fmts[0];
  }

  return AV_SAMPLE_FMT_FLTP;
}

bool codec_supports_samplerate(const AVCodec *codec, int sample_rate) {
  const auto *sample_rates =
      static_cast<const int *>(get_supported_config(codec, AV_CODEC_CONFIG_SAMPLE_RATE));
  if (!sample_rates) {
    return true;
  }
  for (const int *rate = sample_rates; *rate != 0; ++rate) {
    if (*rate == sample_rate) {
      return true;
    }
  }
  return false;
}

bool codec_supports_channels(const AVCodec *codec, int channels) {
  const auto *channel_layouts = static_cast<const AVChannelLayout *>(
      get_supported_config(codec, AV_CODEC_CONFIG_CHANNEL_LAYOUT));
  if (!channel_layouts) {
    return true;
  }
  for (const AVChannelLayout *layout = channel_layouts; layout->nb_channels != 0; ++layout) {
    if (layout->nb_channels == channels) {
      return true;
    }
  }
  return false;
}

bool codec_is_pcm(const AVCodec *codec) {
  return codec && codec->id >= AV_CODEC_ID_PCM_S16LE && codec->id <= AV_CODEC_ID_PCM_F24LE;
}

} // namespace

SingleStreamEncoder::SingleStreamEncoder(const AudioWriteOptions &options)
    : options_(options), packet_(av_packet_alloc()), src_frame_(av_frame_alloc()),
      enc_frame_(av_frame_alloc()) {}

SingleStreamEncoder::~SingleStreamEncoder() { reset(); }

void SingleStreamEncoder::reset() {
  swr_ctx_.reset();
  codec_ctx_.reset();
  packet_.reset(av_packet_alloc());
  src_frame_.reset(av_frame_alloc());
  enc_frame_.reset(av_frame_alloc());
  stream_ = nullptr;
  codec_ = nullptr;
  next_pts_ = 0;
  input_channels_ = 0;
  total_input_samples_ = 0;
  output_buffer_ = nullptr;

  if (fmt_ctx_) {
    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE) && fmt_ctx_->pb) {
      avio_closep(&fmt_ctx_->pb);
    }
    avformat_free_context(fmt_ctx_);
    fmt_ctx_ = nullptr;
  }
}

void SingleStreamEncoder::validate_input(const std::vector<std::vector<float>> &samples,
                                         const std::string &path) const {
  if (path.empty()) {
    throw std::runtime_error("Output path must not be empty");
  }
  if (!options_.sample_rate.has_value() || *options_.sample_rate <= 0) {
    throw std::runtime_error("AudioWriteOptions.sample_rate must be specified and > 0");
  }
  if (samples.empty()) {
    throw std::runtime_error("Audio samples must contain at least one channel");
  }

  const size_t channel_count = samples.size();
  if (options_.num_channels.has_value() &&
      *options_.num_channels != static_cast<int>(channel_count)) {
    throw std::runtime_error("AudioWriteOptions.num_channels does not match input channel count");
  }

  const size_t num_samples = samples.front().size();
  for (const auto &channel : samples) {
    if (channel.size() != num_samples) {
      throw std::runtime_error("All input channels must contain the same number of samples");
    }
  }

  if (!output_buffer_ && !options_.overwrite && std::filesystem::exists(path)) {
    throw std::runtime_error("Output file already exists: " + path);
  }
}

void SingleStreamEncoder::setup_output(const std::string &path,
                                       const std::vector<std::vector<float>> &samples) {
  const char *format_name =
      options_.container_format.has_value() ? options_.container_format->c_str() : nullptr;
  check_av_error(avformat_alloc_output_context2(&fmt_ctx_, nullptr, format_name,
                                                output_buffer_ ? nullptr : path.c_str()),
                 "Could not allocate output context");
  if (!fmt_ctx_) {
    throw std::runtime_error("Could not determine output format for " + path);
  }

  if (options_.codec_name.has_value()) {
    codec_ = avcodec_find_encoder_by_name(options_.codec_name->c_str());
    if (!codec_) {
      throw std::runtime_error("Could not find audio encoder: " + *options_.codec_name);
    }
  } else {
    codec_ = avcodec_find_encoder(fmt_ctx_->oformat->audio_codec);
    if (!codec_) {
      throw std::runtime_error("Could not find default audio encoder for output format");
    }
  }

  stream_ = avformat_new_stream(fmt_ctx_, nullptr);
  if (!stream_) {
    throw std::runtime_error("Could not create output audio stream");
  }

  codec_ctx_.reset(avcodec_alloc_context3(codec_));
  if (!codec_ctx_) {
    throw std::runtime_error("Could not allocate audio encoder context");
  }

  input_channels_ = static_cast<int>(samples.size());
  total_input_samples_ = static_cast<int>(samples.front().size());

  if (!codec_supports_samplerate(codec_, *options_.sample_rate)) {
    throw std::runtime_error("Encoder does not support requested sample rate");
  }
  if (!codec_supports_channels(codec_, input_channels_)) {
    throw std::runtime_error("Encoder does not support requested channel count");
  }

  codec_ctx_->sample_rate = *options_.sample_rate;
  codec_ctx_->time_base = AVRational{1, codec_ctx_->sample_rate};
  codec_ctx_->sample_fmt = choose_encoder_sample_format(codec_, options_.sample_format);
  av_channel_layout_default(&codec_ctx_->ch_layout, input_channels_);
  if (codec_ctx_->ch_layout.nb_channels <= 0) {
    throw std::runtime_error("Could not initialize encoder channel layout");
  }

  if (options_.bit_rate.has_value() && !codec_is_pcm(codec_)) {
    codec_ctx_->bit_rate = *options_.bit_rate;
  }

  if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {
    codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  AVDictionary *codec_opts = nullptr;
  if (codec_->id == AV_CODEC_ID_AAC && !options_.bit_rate.has_value()) {
    av_dict_set_int(&codec_opts, "b", 192000, 0);
  }
  int open_ret = avcodec_open2(codec_ctx_.get(), codec_, &codec_opts);
  if (codec_opts) {
    av_dict_free(&codec_opts);
  }
  check_av_error(open_ret, "Could not open audio encoder");

  check_av_error(avcodec_parameters_from_context(stream_->codecpar, codec_ctx_.get()),
                 "Could not copy audio encoder parameters");
  stream_->time_base = codec_ctx_->time_base;

  if (output_buffer_) {
    auto *avio_buf = static_cast<uint8_t *>(av_malloc(64 * 1024));
    if (!avio_buf) throw std::runtime_error("Could not allocate output buffer");
    fmt_ctx_->pb = avio_alloc_context(avio_buf, 64 * 1024, 1, output_buffer_,
                                      nullptr, write_memory, seek_memory);
    if (!fmt_ctx_->pb) { av_free(avio_buf); throw std::runtime_error("Could not allocate output AVIO context"); }
    fmt_ctx_->flags |= AVFMT_FLAG_CUSTOM_IO;
  } else if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
    check_av_error(avio_open(&fmt_ctx_->pb, path.c_str(), AVIO_FLAG_WRITE),
                   "Could not open output file " + path);
  }

  check_av_error(avformat_write_header(fmt_ctx_, nullptr), "Could not write output header");
}

void SingleStreamEncoder::setup_resampler() {
  SwrContext *swr = nullptr;
  AVChannelLayout input_layout{};
  av_channel_layout_default(&input_layout, input_channels_);
  if (input_layout.nb_channels <= 0) {
    throw std::runtime_error("Could not initialize input channel layout");
  }
  const AVSampleFormat input_sample_fmt = AV_SAMPLE_FMT_FLTP;
  check_av_error(
      swr_alloc_set_opts2(&swr, &codec_ctx_->ch_layout, codec_ctx_->sample_fmt,
                          codec_ctx_->sample_rate, &input_layout, input_sample_fmt,
                          codec_ctx_->sample_rate, 0, nullptr),
      "Could not allocate resampler");
  av_channel_layout_uninit(&input_layout);

  swr_ctx_.reset(swr);
  check_av_error(swr_init(swr_ctx_.get()), "Could not initialize resampler");
}

void SingleStreamEncoder::write_packets() {
  while (true) {
    int ret = avcodec_receive_packet(codec_ctx_.get(), packet_.get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      return;
    }
    check_av_error(ret, "Could not receive encoded audio packet");

    av_packet_rescale_ts(packet_.get(), codec_ctx_->time_base, stream_->time_base);
    packet_->stream_index = stream_->index;
    check_av_error(av_interleaved_write_frame(fmt_ctx_, packet_.get()),
                   "Could not write audio packet");
    av_packet_unref(packet_.get());
  }
}

void SingleStreamEncoder::encode_chunk(const std::vector<std::vector<float>> &samples,
                                       int start_sample,
                                       int num_samples) {
  av_frame_unref(src_frame_.get());
  src_frame_->format = AV_SAMPLE_FMT_FLTP;
  src_frame_->sample_rate = codec_ctx_->sample_rate;
  check_av_error(av_channel_layout_copy(&src_frame_->ch_layout, &codec_ctx_->ch_layout),
                 "Could not copy source channel layout");
  src_frame_->nb_samples = num_samples;
  check_av_error(av_frame_get_buffer(src_frame_.get(), 0),
                 "Could not allocate source frame buffer");
  check_av_error(av_frame_make_writable(src_frame_.get()),
                 "Could not make source frame writable");

  for (int channel = 0; channel < input_channels_; ++channel) {
    const size_t channel_index = static_cast<size_t>(channel);
    const size_t start = static_cast<size_t>(start_sample);
    const size_t count = static_cast<size_t>(num_samples);
    std::copy_n(samples[channel_index].data() + start, count,
                reinterpret_cast<float *>(src_frame_->extended_data[channel_index]));
  }

  AVFrame *frame_to_send = src_frame_.get();
  if (codec_ctx_->sample_fmt != AV_SAMPLE_FMT_FLTP) {
    const int max_out_samples = static_cast<int>(av_rescale_rnd(
        swr_get_delay(swr_ctx_.get(), codec_ctx_->sample_rate) + num_samples,
        codec_ctx_->sample_rate, codec_ctx_->sample_rate, AV_ROUND_UP));

    av_frame_unref(enc_frame_.get());
    enc_frame_->format = codec_ctx_->sample_fmt;
    enc_frame_->sample_rate = codec_ctx_->sample_rate;
    check_av_error(av_channel_layout_copy(&enc_frame_->ch_layout, &codec_ctx_->ch_layout),
                   "Could not copy encoder channel layout");
    enc_frame_->nb_samples = max_out_samples;
    check_av_error(av_frame_get_buffer(enc_frame_.get(), 0),
                   "Could not allocate encoder frame buffer");
    check_av_error(av_frame_make_writable(enc_frame_.get()),
                   "Could not make encoder frame writable");

    int converted = swr_convert(
        swr_ctx_.get(), enc_frame_->extended_data, max_out_samples,
        const_cast<const uint8_t **>(src_frame_->extended_data), num_samples);
    check_av_error(converted, "Could not convert audio samples for encoder");
    enc_frame_->nb_samples = converted;
    frame_to_send = enc_frame_.get();
  }

  frame_to_send->pts = next_pts_;
  next_pts_ += frame_to_send->nb_samples;

  check_av_error(avcodec_send_frame(codec_ctx_.get(), frame_to_send),
                 "Could not send audio frame to encoder");
  write_packets();
}

void SingleStreamEncoder::encode_all_samples(const std::vector<std::vector<float>> &samples) {
  int chunk_size = codec_ctx_->frame_size;
  if (chunk_size <= 0) {
    chunk_size = 4096;
  }

  for (int start = 0; start < total_input_samples_; start += chunk_size) {
    int num_samples = std::min(chunk_size, total_input_samples_ - start);
    encode_chunk(samples, start, num_samples);
  }
}

void SingleStreamEncoder::flush_encoder() {
  check_av_error(avcodec_send_frame(codec_ctx_.get(), nullptr), "Could not flush audio encoder");
  write_packets();
  check_av_error(av_write_trailer(fmt_ctx_), "Could not finalize output file");
}

void SingleStreamEncoder::save(const std::string &path,
                               const std::vector<std::vector<float>> &samples) {
  reset();
  validate_input(samples, path);
  setup_output(path, samples);
  if (codec_ctx_->sample_fmt != AV_SAMPLE_FMT_FLTP) {
    setup_resampler();
  }
  encode_all_samples(samples);
  flush_encoder();
}

std::vector<uint8_t> SingleStreamEncoder::save_buffer(
    const std::vector<std::vector<float>> &samples) {
  reset();
  std::vector<uint8_t> output;
  output_buffer_ = &output;
  std::string format = options_.container_format.value_or("wav");
  if (!options_.container_format) options_.container_format = format;
  if (!options_.codec_name) options_.codec_name = "pcm_s16le";
  validate_input(samples, "memory-output");
  setup_output("memory-output", samples);
  if (codec_ctx_->sample_fmt != AV_SAMPLE_FMT_FLTP) setup_resampler();
  encode_all_samples(samples);
  flush_encoder();
  output_buffer_ = nullptr;
  return output;
}

} // namespace avioflow
