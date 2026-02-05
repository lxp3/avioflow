#include "avioflow-cxx-api.h"
#include "../core/ffmpeg/device-handler.h"
#include "../core/ffmpeg/ffmpeg-common.h"
#include "../core/ffmpeg/single-stream-decoder.h"

namespace avioflow {

// PIMPL Implementation
class AudioDecoder::Impl {
public:
  explicit Impl(const AudioStreamOptions &options) : decoder_(options) {}

  SingleStreamDecoder decoder_;
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

} // namespace avioflow
