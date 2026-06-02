#include "avioflow-cxx-api.h"
#include "../core/ffmpeg/device-handler.h"
#include "../core/ffmpeg/ffmpeg-common.h"
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

void AudioDecoder::open(const std::string &source) {
  impl_->decoder_.open(source);
}

void AudioDecoder::open(const uint8_t *data, size_t size) {
  impl_->decoder_.open(data, size);
}

void AudioDecoder::push(const uint8_t *data, size_t size) {
  impl_->decoder_.push(data, size);
}

void AudioDecoder::finish() { impl_->decoder_.finish(); }

// --- Decoding Methods ---

FrameData AudioDecoder::read() {
  AVFrame *frame = impl_->decoder_.read();
  if (!frame)
    return {nullptr, 0, 0};

  return {reinterpret_cast<float **>(frame->data), frame->ch_layout.nb_channels,
          frame->nb_samples};
}

std::vector<std::vector<float>> AudioDecoder::get_samples() {
  return impl_->decoder_.get_samples();
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

void save_audio(const std::string &path,
                const std::vector<std::vector<float>> &samples,
                const AudioWriteOptions &options) {
  AudioEncoder encoder(options);
  encoder.save(path, samples);
}

} // namespace avioflow
