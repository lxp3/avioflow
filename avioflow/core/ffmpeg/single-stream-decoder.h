
#pragma once

#include "ffmpeg-common.h"
#include "../../include/metadata.h"
#include <optional>
#include <string>
#include <functional>

namespace avioflow
{

  class SingleStreamDecoder
  {
  public:
    explicit SingleStreamDecoder(const AudioStreamOptions &options = {});
    ~SingleStreamDecoder() = default;

    // Open from file path, URL, or device (e.g., "audio=Microphone")
    void open(const std::string &source);

    // Open from memory buffer (e.g., raw encoded audio data with header)
    void open_memory(const uint8_t *data, size_t size);
 
    // Initialize for incremental byte streams with a read callback
    // The callback should return: >0 (bytes read), 0 (EOF), <0 (no data available)
    void open_stream(AVIOReadCallback avio_read_callback);

    // Decode next frame - returns pointer to internal AVFrame
    // WARNING: Data is only valid until the next decode call
    AVFrame *decode_next();

    // Decode entire audio file at once (offline decoding)
    // Returns all samples in planar float format
    AudioSamples get_all_samples();

    // Check if there are more frames to decode
    bool has_more() const { return !eof_reached_; }

    const Metadata &get_metadata() const { return metadata_; }

  private:
    void setup_decoder();
    void setup_resampler(AVFrame *frame);
    int calculate_output_samples(int src_samples, int src_rate, int dst_rate) const;
    AVFrame *process_decoded_frame();

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
    bool needs_resample_ = true;
    bool resampler_initialized_ = false;

    // Data provider callback for streaming
    AVIOReadCallback avio_read_callback_;

    // static
    static constexpr AVSampleFormat output_sample_format_ = AV_SAMPLE_FMT_FLTP;
  };

} // namespace avioflow
