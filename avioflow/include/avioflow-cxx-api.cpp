#include "avioflow-cxx-api.h"
#include "../core/ffmpeg/device-handler.h"
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

void AudioDecoder::push(const uint8_t *data, size_t size) {
  impl_->decoder_.push(data, size);
}

// --- Decoding Methods ---

FrameData AudioDecoder::decode_next() {
  AVFrame *frame = impl_->decoder_.decode_next();
  if (!frame)
    return {nullptr, 0, 0};
  
  return {
    reinterpret_cast<float**>(frame->data),
    frame->ch_layout.nb_channels,
    frame->nb_samples
  };
}

std::vector<std::vector<float>> AudioDecoder::get_all_samples() {
  return impl_->decoder_.get_all_samples();
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
  std::string log_level_str;

  if (level == nullptr) {
    const char *env_level = std::getenv("AVIOFLOW_LOG_LEVEL");
    if (env_level != nullptr) {
      log_level_str = env_level;
    } else {
      log_level_str = "info";
    }
  } else {
    log_level_str = level;
  }

  for (auto &c : log_level_str) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  int av_level = AV_LOG_INFO;
  if (log_level_str == "quiet")
    av_level = AV_LOG_QUIET;
  else if (log_level_str == "panic")
    av_level = AV_LOG_PANIC;
  else if (log_level_str == "fatal")
    av_level = AV_LOG_FATAL;
  else if (log_level_str == "error")
    av_level = AV_LOG_ERROR;
  else if (log_level_str == "warning")
    av_level = AV_LOG_WARNING;
  else if (log_level_str == "info")
    av_level = AV_LOG_INFO;
  else if (log_level_str == "verbose")
    av_level = AV_LOG_VERBOSE;
  else if (log_level_str == "debug")
    av_level = AV_LOG_DEBUG;
  else if (log_level_str == "trace")
    av_level = AV_LOG_TRACE;

  av_log_set_level(av_level);
}

} // namespace avioflow
