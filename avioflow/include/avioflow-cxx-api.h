#pragma once

#include "metadata.h"
#include <memory>
#include <vector>

namespace avioflow {

// Global configuration
// Defaults to "error" when AVIOFLOW_LOG_LEVEL is not set.
// level: "quiet", "panic", "fatal", "error", "warning", "info", "verbose",
// "debug", "trace"
AVIOFLOW_API void avioflow_set_log_level(const char *level = nullptr);

/**
 * @brief Get list of supported audio decoder names.
 * @return Vector of decoder names (e.g., "mp3", "aac", "pcm_s16le")
 */
AVIOFLOW_API std::vector<std::string> get_supported_decoders();

/**
 * @brief Get list of supported audio encoder names.
 * @return Vector of encoder names (e.g., "pcm_s16le", "aac", "libmp3lame")
 */
AVIOFLOW_API std::vector<std::string> get_supported_encoders();

/**
 * @brief Get list of supported input format (demuxer) names.
 * @return Vector of demuxer names (e.g., "mp3", "wav", "flac", "s16le")
 */
AVIOFLOW_API std::vector<std::string> get_supported_input_formats();

/**
 * @brief Get list of supported output format (muxer) names.
 * @return Vector of muxer names (e.g., "wav", "flac", "mp4", "adts")
 */
AVIOFLOW_API std::vector<std::string> get_supported_output_formats();

/**
 * @brief Raw audio frame data returned from decoder.
 *
 * Points directly to internal AVFrame buffers - zero allocation overhead.
 *
 * @warning Data is only valid until the next decode call. Copy immediately if
 * needed.
 * @note No manual memory management required - owned by AudioDecoder.
 */
struct FrameData {
  float **data;     ///< Planar channel pointers (data[channel][sample])
  int num_channels; ///< Number of audio channels
  int num_samples;  ///< Number of samples per channel

  /// @brief Check if frame contains valid data
  explicit operator bool() const { return data != nullptr && num_samples > 0; }
};

/**
 * @brief High-performance audio decoder powered by FFmpeg.
 *
 * Supports two modes:
 * - **Offline mode**: Load from file path, URL, memory, or device.
 * - **Stream mode**: Feed bytes and pull currently available decoded data.
 *
 * Example (File mode):
 * @code
 * AudioDecoder decoder({.output_sample_rate = 44100});
 * decoder.load_file("audio.mp3");
 * while (auto frame = decoder.get_frame()) {
 *   process(frame.data, frame.num_channels, frame.num_samples);
 * }
 * @endcode
 *
 * Example (Stream mode):
 * @code
 * AudioDecoder decoder({.input_format = "s16le", .input_sample_rate = 48000});
 * decoder.feed(raw_bytes, size);
 * while (auto frame = decoder.get_frame()) { ... }
 * decoder.flush();
 * @endcode
 */
class AVIOFLOW_API AudioDecoder {
public:
  explicit AudioDecoder(const AudioStreamOptions &options = {});
  ~AudioDecoder();

  // Non-copyable
  AudioDecoder(const AudioDecoder &) = delete;
  AudioDecoder &operator=(const AudioDecoder &) = delete;

  // Movable
  AudioDecoder(AudioDecoder &&) noexcept;
  AudioDecoder &operator=(AudioDecoder &&) noexcept;

  // === Input Methods ===

  /**
   * @brief Open audio from file path, URL, or device.
   * @param source File path, URL, or device identifier (e.g.,
   * "audio=Microphone")
   * @throws std::runtime_error if source cannot be opened
   */
  Metadata load_file(const std::string &source);

  /**
   * @brief Open audio from memory (full bytes).
   * @param data Pointer to full audio file bytes
   * @param size Number of bytes
   * @throws std::runtime_error if data cannot be parsed
   */
  Metadata load_buffer(const uint8_t *data, size_t size);

  /**
   * @brief Push raw bytes for streaming decode.
   *
   * First call auto-initializes the streaming context using constructor
   * options. Requires input_format to be set in options.
   *
   * @param data Pointer to raw encoded audio bytes
   * @param size Number of bytes to push
   * @throws std::runtime_error if input_format not specified or decoder in file
   * mode
   */
  void feed(const uint8_t *data, size_t size);

  /**
   * @brief Mark push-based streaming input as complete.
   *
   * After calling flush(), get_frame()/get_samples() drains buffered and
   * decoder-delayed frames until is_finished() becomes true.
   */
  void flush();

  // === Decoding Methods ===

  /**
   * @brief Decode next audio frame.
   *
   * Returns raw pointers to internal buffers for zero-copy access.
   *
   * @return FrameData with valid pointers, or empty FrameData (bool() == false)
   * on EOF/no data
   * @warning Returned data is only valid until next get_frame()/get_samples()
   * call!
   */
  FrameData get_frame();

  /**
   * @brief Decode all currently available samples.
   *
   * In File Mode: Decodes until EOF.
   * In Stream Mode: Decodes all buffered data until more input is required.
   *
   * @return All samples as vector[channel][sample]
   */
  std::vector<std::vector<float>> get_samples();

  // === Status ===

  bool is_finished() const;
  const Metadata &get_metadata() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Offline audio encoder that writes in-memory samples to a file.
 *
 * Input samples use planar float layout: samples[channel][sample].
 */
class AVIOFLOW_API AudioEncoder {
public:
  explicit AudioEncoder(const AudioWriteOptions &options = {});
  ~AudioEncoder();

  AudioEncoder(const AudioEncoder &) = delete;
  AudioEncoder &operator=(const AudioEncoder &) = delete;

  AudioEncoder(AudioEncoder &&) noexcept;
  AudioEncoder &operator=(AudioEncoder &&) noexcept;

  void save(const std::string &path,
            const std::vector<std::vector<float>> &samples);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Convenience helper to save planar float audio samples to a file.
 */
AVIOFLOW_API void save_audio(const std::string &path,
                             const std::vector<std::vector<float>> &samples,
                             const AudioWriteOptions &options = {});

// Device Manager for hardware discovery
class AVIOFLOW_API DeviceManager {
public:
  static std::vector<DeviceInfo> list_audio_devices();
};

} // namespace avioflow
