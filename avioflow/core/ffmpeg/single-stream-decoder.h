
#pragma once

#include "ffmpeg-common.h"
#include "metadata.h"
#ifdef AVIOFLOW_HAS_WASAPI
#include "wasapi-handler.h"
#endif
#include <optional>
#include <string>
#include <functional>
#include <mutex>

namespace avioflow
{

  class SingleStreamDecoder
  {
  public:
    explicit SingleStreamDecoder(const AudioStreamOptions &options = {});
    ~SingleStreamDecoder() = default;

    // === Input Methods ===

    // Mode 1: Open from file path, URL, or device (e.g., "audio=Microphone")
    void load_file(const std::string &source);

    // Mode 1b: Open from memory (full bytes)
    void load_buffer(const uint8_t *data, size_t size);

    // Mode 2: Push-based streaming
    // - Requires input_format to be set in options
    void feed(const uint8_t *data, size_t size);
    void flush();

    // === Decoding ===

    // Decode next frame - returns pointer to internal AVFrame
    // WARNING: Data is only valid until the next read call
    AVFrame *get_frame();

    // Decode samples in the half-open range [start_seconds, stop_seconds).
    // Offline mode only. May be called multiple times on the same decoder to
    // fetch different ranges (each call seeks independently). start_seconds
    // defaults to the beginning; stop_seconds defaults to the end.
    std::vector<std::vector<float>> get_samples(double start_seconds = 0.0,
                                                std::optional<double> stop_seconds = std::nullopt);

    // === Status ===

    bool is_finished() const { return eof_reached_; }
    const Metadata &get_metadata() const { return metadata_; }

  private:
    enum class Mode { None, Offline, Stream };
    Mode mode_ = Mode::None;

    // Stream mode buffer
    std::vector<uint8_t> push_buffer_;
    std::mutex buffer_mtx_;
    bool input_finished_ = false;

    // Internal helpers
    void init_stream_context();
    void setup_decoder();
    void seek_to(double start_seconds);
    enum class TrimAction { Keep, Skip, Stop };
    TrimAction trim_to_range(AVFrame *frame, double start_seconds,
                             const std::optional<double> &stop_seconds);
    AVFrame *decode_next_frame();
    AVFrame *read_raw_pcm_frame();
    void setup_resampler(AVFrame *frame);
    int calculate_output_samples(int src_samples, int src_rate, int dst_rate) const;
    AVFrame *process_decoded_frame();
    void update_decoded_metadata(const AVFrame *frame);
    static void validate_options(const AudioStreamOptions &options);

    // Core FFmpeg contexts
    AVFormatContextPtr fmt_ctx_;
    AVCodecContextPtr codec_ctx_;
    SwrContextPtr swr_ctx_;

    AVPacketPtr packet_;
    AVFramePtr frame_;
    AVFramePtr converted_frame_;

    AudioStreamOptions options_;
    Metadata metadata_;
    int audio_stream_index_ = -1;
    bool eof_reached_ = false;
    size_t stream_read_offset_ = 0;
    bool needs_resample_ = true;
    bool resampler_initialized_ = false;
    int64_t total_samples_decoded_ = 0;

#ifdef AVIOFLOW_HAS_WASAPI
    std::unique_ptr<WasapiHandler> wasapi_handler_;
    bool is_wasapi_mode_ = false;
#endif

    static constexpr AVSampleFormat output_sample_format_ = AV_SAMPLE_FMT_FLTP;
  };

} // namespace avioflow
