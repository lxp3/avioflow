
#include "single-stream-decoder.h"
#include "io-handler.h"
#include "device-handler.h"
#include <iostream>

namespace avioflow {

SingleStreamDecoder::SingleStreamDecoder() 
    : packet_(av_packet_alloc()), frame_(av_frame_alloc()) {
}

void SingleStreamDecoder::open(const std::string& source) {
    if (source.find("audio=") == 0 || source.find("video=") == 0) {
        fmt_ctx_.reset(DeviceHandler::open_device(source));
    } else {
        fmt_ctx_.reset(IOHandler::open_url(source));
    }
    setup_decoder();
}

void SingleStreamDecoder::open(const uint8_t* data, size_t size) {
    fmt_ctx_.reset(IOHandler::open_memory(data, size));
    setup_decoder();
}

void SingleStreamDecoder::setup_decoder() {
    check_av_error(avformat_find_stream_info(fmt_ctx_.get(), nullptr), "Could not find stream info");

    audio_stream_index_ = av_find_best_stream(fmt_ctx_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_index_ < 0) throw std::runtime_error("Could not find audio stream");

    AVStream* stream = fmt_ctx_->streams[audio_stream_index_];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) throw std::runtime_error("Could not find decoder");

    codec_ctx_.reset(avcodec_alloc_context3(codec));
    check_av_error(avcodec_parameters_to_context(codec_ctx_.get(), stream->codecpar), "Could not copy codec params");
    check_av_error(avcodec_open2(codec_ctx_.get(), codec, nullptr), "Could not open codec");

    // Populate metadata
    metadata_.sample_rate = codec_ctx_->sample_rate;
    metadata_.num_channels = codec_ctx_->ch_layout.nb_channels;
    metadata_.duration = (double)fmt_ctx_->duration / AV_TIME_BASE;
    metadata_.format = fmt_ctx_->iformat->name;
    
    setup_resampler();
}

void SingleStreamDecoder::setup_resampler() {
    SwrContext* swr = nullptr;
    check_av_error(swr_alloc_set_opts2(&swr,
        &codec_ctx_->ch_layout, TARGET_FORMAT, codec_ctx_->sample_rate,
        &codec_ctx_->ch_layout, codec_ctx_->sample_fmt, codec_ctx_->sample_rate,
        0, nullptr), "Could not initialize resampler");
    
    swr_ctx_.reset(swr);
    check_av_error(swr_init(swr_ctx_.get()), "Could not initialize resampler context");
}

std::vector<std::vector<float>> SingleStreamDecoder::decode_next() {
    while (av_read_frame(fmt_ctx_.get(), packet_.get()) >= 0) {
        if (packet_->stream_index == audio_stream_index_) {
            int ret = avcodec_send_packet(codec_ctx_.get(), packet_.get());
            av_packet_unref(packet_.get());
            if (ret < 0) continue;

            ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_unref(frame_.get());
                continue;
            } else if (ret < 0) {
                av_frame_unref(frame_.get());
                throw std::runtime_error("Error during decoding");
            }

            auto result = process_frame(frame_.get());
            av_frame_unref(frame_.get());
            return result;
        }
        av_packet_unref(packet_.get());
    }
    return {}; // EOF
}

std::vector<std::vector<float>> SingleStreamDecoder::get_all_samples() {
    std::vector<std::vector<float>> all_samples(metadata_.num_channels);
    
    // Seek to start for safety
    avformat_seek_file(fmt_ctx_.get(), audio_stream_index_, 0, 0, 0, 0);
    avcodec_flush_buffers(codec_ctx_.get());

    while (true) {
        auto frame_samples = decode_next();
        if (frame_samples.empty()) break;
        
        for (int c = 0; c < metadata_.num_channels; ++c) {
            all_samples[c].insert(all_samples[c].end(), frame_samples[c].begin(), frame_samples[c].end());
        }
    }
    
    return all_samples;
}

std::vector<std::vector<float>> SingleStreamDecoder::process_frame(AVFrame* frame) {
    int num_channels = frame->ch_layout.nb_channels;
    int num_samples = frame->nb_samples;

    std::vector<std::vector<float>> output(num_channels, std::vector<float>(num_samples));
    
    // Pointers for Swr
    std::vector<uint8_t*> out_data(num_channels);
    for (int i = 0; i < num_channels; ++i) {
        out_data[i] = reinterpret_cast<uint8_t*>(output[i].data());
    }

    int ret = swr_convert(swr_ctx_.get(), out_data.data(), num_samples, (const uint8_t**)frame->data, num_samples);
    if (ret < 0) throw std::runtime_error("Error during resampling");

    return output;
}

} // namespace avioflow
