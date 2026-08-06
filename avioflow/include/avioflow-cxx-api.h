#pragma once

#include "metadata.h"
#include <memory>
#include <optional>
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
 * Example (Decode only a time range, e.g. seconds 10.3 to 20.3):
 * @code
 * AudioDecoder decoder;
 * decoder.load_file("audio.mp3");
 * auto samples = decoder.get_samples(10.3, 20.3); // only the requested range
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
   * @brief Decode samples in the half-open range [start_seconds, stop_seconds).
   *
   * In File Mode: decodes the requested range (or until EOF if stop_seconds
   * is unset). May be called multiple times on the same decoder to fetch
   * different ranges; each call seeks independently.
   * In Stream Mode: start_seconds/stop_seconds are not supported; decodes all
   * buffered data until more input is required.
   *
   * @param start_seconds Range start in seconds (offline mode only). Defaults
   * to the beginning.
   * @param stop_seconds Range end in seconds, exclusive (offline mode only).
   * Defaults to the end.
   * @throws std::invalid_argument if start_seconds < 0 or stop_seconds <=
   * start_seconds
   * @return Samples in the requested range as vector[channel][sample]
   */
  std::vector<std::vector<float>> get_samples(
      double start_seconds = 0.0,
      std::optional<double> stop_seconds = std::nullopt);

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

/**
 * @brief Stateful resampler for in-memory planar float samples.
 *
 * Keeps resampler state across calls, so feeding consecutive chunks produces
 * output that joins seamlessly. Use this when audio arrives in pieces; for a
 * single buffer you already hold in full, prefer the `resample()` helper.
 *
 * Input and output layout is planar float: samples[channel][sample].
 *
 * Example (chunked):
 * @code
 * AudioResampler resampler({.input_sample_rate = 44100,
 *                           .output_sample_rate = 16000});
 * std::vector<std::vector<float>> out(1);
 * for (const auto &chunk : chunks) {
 *   auto part = resampler.process(chunk);
 *   for (size_t c = 0; c < part.size(); ++c)
 *     out[c].insert(out[c].end(), part[c].begin(), part[c].end());
 * }
 * auto tail = resampler.flush(); // required, else the tail is lost
 * @endcode
 */
class AVIOFLOW_API AudioResampler {
public:
  explicit AudioResampler(const AudioResampleOptions &options);
  ~AudioResampler();

  AudioResampler(const AudioResampler &) = delete;
  AudioResampler &operator=(const AudioResampler &) = delete;

  AudioResampler(AudioResampler &&) noexcept;
  AudioResampler &operator=(AudioResampler &&) noexcept;

  /**
   * @brief Resample one chunk of planar float samples.
   *
   * The returned chunk may hold fewer samples than the rate ratio suggests,
   * because the resampler buffers samples internally to keep filter continuity.
   *
   * @param samples Input as samples[channel][sample]; all channels must be
   * the same length, and the channel count must not change between calls.
   * @return Resampled samples as vector[channel][sample]
   * @throws std::invalid_argument on ragged channels or a channel-count change
   */
  std::vector<std::vector<float>>
  process(const std::vector<std::vector<float>> &samples);

  /**
   * @brief Drain samples still buffered inside the resampler.
   *
   * Call once after the final process() call. Skipping this drops the last
   * few milliseconds of audio.
   *
   * @return Remaining samples as vector[channel][sample]
   */
  std::vector<std::vector<float>> flush();

  /// Output sample rate the resampler was configured with.
  int output_sample_rate() const;

  /// Output channel count. Zero until the first process() call determines it.
  int output_num_channels() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * @brief Resample a complete planar float buffer in one call.
 *
 * Handles the internal flush, so no samples are lost. For audio arriving in
 * chunks use AudioResampler instead, since calling this per chunk would reset
 * filter state and introduce discontinuities at every boundary.
 *
 * Example:
 * @code
 * auto out = resample(samples, 44100, 16000);        // keep channel count
 * auto mono = resample(samples, 44100, 16000, 1);    // also downmix to mono
 * @endcode
 *
 * @param samples Input as samples[channel][sample]
 * @param input_sample_rate Source sample rate in Hz, must be > 0
 * @param output_sample_rate Target sample rate in Hz, must be > 0
 * @param output_num_channels Target channel count; defaults to the input count
 * @return Resampled samples as vector[channel][sample]
 * @throws std::invalid_argument on non-positive rates or ragged channels
 */
AVIOFLOW_API std::vector<std::vector<float>>
resample(const std::vector<std::vector<float>> &samples,
         int input_sample_rate, int output_sample_rate,
         std::optional<int> output_num_channels = std::nullopt);

// Device Manager for hardware discovery
class AVIOFLOW_API DeviceManager {
public:
  static std::vector<DeviceInfo> list_audio_devices();
};

} // namespace avioflow
