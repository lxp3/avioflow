#include "avioflow-cxx-api.h"
#include "../core/ffmpeg/device-handler.h"
#include "../core/ffmpeg/single-stream-decoder.h"


namespace avioflow {

// PIMPL Implementation
class AudioDecoder::Impl {
public:
  explicit Impl(const AudioStreamOptions &options) : decoder_(options) {}

  SingleStreamDecoder decoder_;
  Metadata cached_metadata_;
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
  impl_->cached_metadata_ = impl_->decoder_.get_metadata();
}

void AudioDecoder::open_memory(const uint8_t *data, size_t size) {
  impl_->decoder_.open_memory(data, size);
  impl_->cached_metadata_ = impl_->decoder_.get_metadata();
}

void AudioDecoder::open_stream(AVIOReadCallback avio_read_callback, const AudioStreamOptions &options) {
  // Validate that format is specified
  if (!options.input_format.has_value()) {
    throw std::runtime_error("input_format must be specified for streaming (e.g., aac, opus, pcm_s16le, wav)");
  }
  
  // Validate supported streaming formats
  const std::string& format = options.input_format.value();
  if (format != "aac" && format != "opus" && format != "pcm_s16le" && 
      format != "pcm_f32le" && format != "wav" && format != "adts") {
    throw std::runtime_error("Unsupported streaming format: " + format + 
                           ". Supported: aac, opus, pcm_s16le, pcm_f32le, wav");
  }
  
  // Recreate impl with streaming options
  impl_ = std::make_unique<Impl>(options);
  impl_->decoder_.open_stream(std::move(avio_read_callback));
  impl_->cached_metadata_ = impl_->decoder_.get_metadata();
}

// --- Decoding Methods ---

AudioSamples AudioDecoder::decode_next() {
  AudioSamples result;

  AVFrame *frame = impl_->decoder_.decode_next();
  if (!frame)
    return result; // Empty samples

  result.sample_rate = frame->sample_rate;
  int num_channels = frame->ch_layout.nb_channels;
  result.data.resize(num_channels);

  for (int c = 0; c < num_channels; ++c) {
    const float *channel_data = reinterpret_cast<const float *>(frame->data[c]);
    result.data[c].assign(channel_data, channel_data + frame->nb_samples);
  }

  return result;
}

AudioSamples AudioDecoder::get_all_samples() {
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
      // Default level if neither parameter nor env var is set
      log_level_str = "info";
    }
  } else {
    log_level_str = level;
  }

  // Convert to lowercase for comparison
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
