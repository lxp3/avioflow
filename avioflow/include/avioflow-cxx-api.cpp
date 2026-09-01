#include "avioflow-cxx-api.h"
#include "../core/ffmpeg/device-handler.h"
#include "../core/ffmpeg/ffmpeg-common.h"
#include "../core/ffmpeg/audio-resampler.h"
#include "../core/ffmpeg/single-stream-decoder.h"
#include "../core/ffmpeg/single-stream-encoder.h"

namespace avioflow {

// PIMPL Implementation
class AudioDecoder::Impl {
public:
  explicit Impl(const AudioStreamOptions &options) : decoder_(options) {}

  SingleStreamDecoder decoder_;
};

class AudioEncoder::Impl {
public:
  explicit Impl(const AudioWriteOptions &options) : encoder_(options) {}

  SingleStreamEncoder encoder_;
};

class AudioResampler::Impl {
public:
  explicit Impl(const AudioResampleOptions &options) : resampler_(options) {}

  SingleStreamResampler resampler_;
};

// Constructor
AudioDecoder::AudioDecoder(const AudioStreamOptions &options)
    : impl_(std::make_unique<Impl>(options)) {}

// Destructor
AudioDecoder::~AudioDecoder() = default;

// Move constructor
AudioDecoder::AudioDecoder(AudioDecoder &&) noexcept = default;

// Move assignment
AudioDecoder &AudioDecoder::operator=(AudioDecoder &&) noexcept = default;

// --- Input Methods ---

Metadata AudioDecoder::load_file(const std::string &source) {
  impl_->decoder_.load_file(source);
  return impl_->decoder_.get_metadata();
}

Metadata AudioDecoder::load_buffer(const uint8_t *data, size_t size) {
  impl_->decoder_.load_buffer(data, size);
  return impl_->decoder_.get_metadata();
}

void AudioDecoder::feed(const uint8_t *data, size_t size) {
  impl_->decoder_.feed(data, size);
}

void AudioDecoder::flush() { impl_->decoder_.flush(); }

// --- Decoding Methods ---

FrameData AudioDecoder::get_frame() {
  AVFrame *frame = impl_->decoder_.get_frame();
  if (!frame)
    return {nullptr, 0, 0};

  return {reinterpret_cast<float **>(frame->data), frame->ch_layout.nb_channels,
          frame->nb_samples};
}

std::vector<std::vector<float>> AudioDecoder::get_samples(
    double start_seconds, std::optional<double> stop_seconds) {
  return impl_->decoder_.get_samples(start_seconds, stop_seconds);
}

// --- Status ---

bool AudioDecoder::is_finished() const { return impl_->decoder_.is_finished(); }

const Metadata &AudioDecoder::get_metadata() const {
  return impl_->decoder_.get_metadata();
}

// --- Device Manager ---

std::vector<DeviceInfo> DeviceManager::list_audio_devices() {
  return DeviceHandler::list_devices();
}

// Global configuration
void avioflow_set_log_level(const char *level) {
  internal_set_log_level(level);
}

std::vector<std::string> get_supported_decoders() {
  return internal_get_supported_decoders();
}

std::vector<std::string> get_supported_encoders() {
  return internal_get_supported_encoders();
}

std::vector<std::string> get_supported_input_formats() {
  return internal_get_supported_input_formats();
}

std::vector<std::string> get_supported_output_formats() {
  return internal_get_supported_output_formats();
}

AudioWriteOptions::AudioWriteOptions(const std::string &format,
                                     std::optional<int> sample_rate_,
                                     std::optional<int> num_channels_,
                                     std::optional<int64_t> bit_rate_)
    : sample_rate(sample_rate_), num_channels(num_channels_), bit_rate(bit_rate_) {
  if (format == "wav") {
    codec_name = "pcm_s16le";
    container_format = "wav";
  } else if (format == "flac") {
    codec_name = "flac";
    container_format = "flac";
  } else if (format == "aac") {
    codec_name = "aac";
    container_format = "adts";
    if (!bit_rate) bit_rate = 192000;
  } else if (format == "mp3") {
    codec_name = "libmp3lame";
    container_format = "mp3";
    if (!bit_rate) bit_rate = 192000;
  } else if (format == "opus") {
    codec_name = "libopus";
    container_format = "opus";
    if (!bit_rate) bit_rate = 128000;
  }
}

AudioEncoder::AudioEncoder(const AudioWriteOptions &options)
    : impl_(std::make_unique<Impl>(options)) {}

AudioEncoder::~AudioEncoder() = default;

AudioEncoder::AudioEncoder(AudioEncoder &&) noexcept = default;

AudioEncoder &AudioEncoder::operator=(AudioEncoder &&) noexcept = default;

void AudioEncoder::save(const std::string &path,
                        const std::vector<std::vector<float>> &samples) {
  impl_->encoder_.save(path, samples);
}

std::vector<uint8_t> AudioEncoder::save_buffer(
    const std::vector<std::vector<float>> &samples) {
  return impl_->encoder_.save_buffer(samples);
}

void save_audio(const std::string &path,
                const std::vector<std::vector<float>> &samples,
                const AudioWriteOptions &options) {
  AudioEncoder encoder(options);
  encoder.save(path, samples);
}

std::vector<uint8_t> save_audio_buffer(
    const std::vector<std::vector<float>> &samples,
    const AudioWriteOptions &options) {
  AudioEncoder encoder(options);
  return encoder.save_buffer(samples);
}

// --- AudioResampler ---

AudioResampler::AudioResampler(const AudioResampleOptions &options)
    : impl_(std::make_unique<Impl>(options)) {}

AudioResampler::~AudioResampler() = default;

AudioResampler::AudioResampler(AudioResampler &&) noexcept = default;

AudioResampler &AudioResampler::operator=(AudioResampler &&) noexcept = default;

std::vector<std::vector<float>>
AudioResampler::process(const std::vector<std::vector<float>> &samples) {
  return impl_->resampler_.process(samples);
}

std::vector<std::vector<float>> AudioResampler::flush() {
  return impl_->resampler_.flush();
}

int AudioResampler::output_sample_rate() const {
  return impl_->resampler_.output_sample_rate();
}

int AudioResampler::output_num_channels() const {
  return impl_->resampler_.output_num_channels();
}

std::vector<std::vector<float>>
resample(const std::vector<std::vector<float>> &samples,
         int input_sample_rate, int output_sample_rate,
         std::optional<int> output_num_channels) {
  AudioResampleOptions options;
  options.input_sample_rate = input_sample_rate;
  options.output_sample_rate = output_sample_rate;
  options.output_num_channels = output_num_channels;

  AudioResampler resampler(options);
  auto result = resampler.process(samples);
  auto tail = resampler.flush();

  if (result.empty())
    return tail;

  for (size_t c = 0; c < result.size() && c < tail.size(); ++c)
    result[c].insert(result[c].end(), tail[c].begin(), tail[c].end());
  return result;
}

} // namespace avioflow
