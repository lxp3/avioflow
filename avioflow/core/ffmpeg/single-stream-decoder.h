
#pragma once

#include "ffmpeg-common.h"
#include "metadata.h"
#include <vector>
#include <string>

namespace avioflow {

class SingleStreamDecoder {
public:
    SingleStreamDecoder();
    ~SingleStreamDecoder() = default;

    // Direct opening
    void open(const std::string& source);
    void open(const uint8_t* data, size_t size);

    // Decoding
    std::vector<std::vector<float>> decode_next();
    std::vector<std::vector<float>> get_all_samples();

    const Metadata& get_metadata() const { return metadata_; }

private:
    void setup_decoder();
    void setup_resampler();
    std::vector<std::vector<float>> process_frame(AVFrame* frame);

    AVFormatContextPtr fmt_ctx_;
    AVCodecContextPtr codec_ctx_;
    SwrContextPtr swr_ctx_;
    
    AVPacketPtr packet_;
    AVFramePtr frame_;

    Metadata metadata_;
    int audio_stream_index_ = -1;

    // Target output format
    static constexpr AVSampleFormat TARGET_FORMAT = AV_SAMPLE_FMT_FLTP;
};

} // namespace avioflow
