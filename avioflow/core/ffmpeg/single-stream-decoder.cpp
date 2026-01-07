#include "single-stream-decoder.h"
#include "avio-context-handler.h"
#include "device-handler.h"
#include <mutex>
#include <functional>

namespace avioflow
{

  SingleStreamDecoder::SingleStreamDecoder(const AudioStreamOptions &options)
      : packet_(av_packet_alloc()), frame_(av_frame_alloc()),
        converted_frame_(av_frame_alloc()), options_(options) {}

  void SingleStreamDecoder::open(const std::string &source)
  {
    if (source.find("audio=") == 0 || source.find("video=") == 0)
    {
      fmt_ctx_.reset(DeviceHandler::open_device(source));
    }
    else
    {
      fmt_ctx_.reset(AvioContextHandler::open_url(source));
    }
    setup_decoder();
  }

  void SingleStreamDecoder::open_memory(const uint8_t *data, size_t size)
  {
    fmt_ctx_.reset(AvioContextHandler::open_memory(data, size, options_));
    setup_decoder();
  }

  void SingleStreamDecoder::open_stream(AVIOReadCallback avio_read_callback)
  {
    avio_read_callback_ = std::move(avio_read_callback);
    fmt_ctx_.reset(AvioContextHandler::open_stream(avio_read_callback_, options_));
    setup_decoder();
  }

  void SingleStreamDecoder::setup_decoder()
  {
    check_av_error(avformat_find_stream_info(fmt_ctx_.get(), nullptr),
                   "Could not find stream info");

    audio_stream_index_ = av_find_best_stream(fmt_ctx_.get(), AVMEDIA_TYPE_AUDIO,
                                              -1, -1, nullptr, 0);
    if (audio_stream_index_ < 0)
      throw std::runtime_error("Could not find audio stream");

    AVStream *stream = fmt_ctx_->streams[audio_stream_index_];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec)
      throw std::runtime_error("Could not find decoder");

    codec_ctx_.reset(avcodec_alloc_context3(codec));
    check_av_error(
        avcodec_parameters_to_context(codec_ctx_.get(), stream->codecpar),
        "Could not copy codec params");
    check_av_error(avcodec_open2(codec_ctx_.get(), codec, nullptr),
                   "Could not open codec");

    // Populate metadata
    metadata_.sample_rate = codec_ctx_->sample_rate;
    metadata_.num_channels = codec_ctx_->ch_layout.nb_channels;
    
    // Safer duration calculation to avoid overflow/garbage values
    if (fmt_ctx_->duration != AV_NOPTS_VALUE) {
        metadata_.duration = static_cast<double>(fmt_ctx_->duration) / AV_TIME_BASE;
    } else if (stream->duration != AV_NOPTS_VALUE) {
        metadata_.duration = static_cast<double>(stream->duration) * av_q2d(stream->time_base);
    } else {
        metadata_.duration = 0.0;
    }

    metadata_.sample_format = fmt_ctx_->iformat->name;

    total_samples_decoded_ = 0;
    eof_reached_ = false;
    resampler_initialized_ = false;
  }

  void SingleStreamDecoder::setup_resampler(AVFrame *frame)
  {
    int src_sample_rate = frame->sample_rate;
    AVSampleFormat src_sample_format = static_cast<AVSampleFormat>(frame->format);
    int src_num_channels = frame->ch_layout.nb_channels;

    int out_rate = options_.output_sample_rate.value_or(src_sample_rate);
    int out_channels = options_.output_num_channels.value_or(src_num_channels);

    needs_resample_ = (src_sample_format != output_sample_format_) ||
                      (src_sample_rate != out_rate) ||
                      (src_num_channels != out_channels);

    if (needs_resample_)
    {
      AVChannelLayout out_ch_layout;
      av_channel_layout_default(&out_ch_layout, out_channels);

      SwrContext *swr = nullptr;
      check_av_error(
          swr_alloc_set_opts2(&swr, &out_ch_layout, output_sample_format_,
                              out_rate, &frame->ch_layout,
                              src_sample_format, src_sample_rate, 0, nullptr),
          "Could not initialize resampler");

      swr_ctx_.reset(swr);
      check_av_error(swr_init(swr_ctx_.get()),
                     "Could not initialize resampler context");

      av_channel_layout_uninit(&out_ch_layout);
    }

    resampler_initialized_ = true;
  }

  int SingleStreamDecoder::calculate_output_samples(int src_samples,
                                                    int src_rate,
                                                    int dst_rate) const
  {
    if (src_rate == dst_rate)
      return src_samples;
    int64_t delay = swr_ctx_ ? swr_get_delay(swr_ctx_.get(), src_rate) : 0;
    return static_cast<int>(av_rescale_rnd(
        delay + src_samples, dst_rate, src_rate, AV_ROUND_UP));
  }

  AVFrame *SingleStreamDecoder::process_decoded_frame()
  {
    if (!resampler_initialized_)
      setup_resampler(frame_.get());

    if (needs_resample_)
    {
      int out_rate = options_.output_sample_rate.value_or(frame_->sample_rate);
      int out_channels = options_.output_num_channels.value_or(frame_->ch_layout.nb_channels);
      int out_samples = calculate_output_samples(frame_->nb_samples, frame_->sample_rate, out_rate);

      av_frame_unref(converted_frame_.get());
      converted_frame_->format = output_sample_format_;
      converted_frame_->sample_rate = out_rate;
      av_channel_layout_default(&converted_frame_->ch_layout, out_channels);
      converted_frame_->nb_samples = out_samples;

      check_av_error(av_frame_get_buffer(converted_frame_.get(), 0),
                     "Could not allocate converted frame buffer");

      int converted = swr_convert(
          swr_ctx_.get(), converted_frame_->data, out_samples,
          const_cast<const uint8_t **>(frame_->extended_data),
          frame_->nb_samples);

      if (converted < 0)
      {
        av_frame_unref(frame_.get());
        throw std::runtime_error("Error during resampling");
      }

      converted_frame_->nb_samples = converted;
      return converted_frame_.get();
    }
    else
    {
      return frame_.get();
    }
  }

  AVFrame *SingleStreamDecoder::decode_next()
  {
    while (true)
    {
      // 1. Try to receive frame from decoder first (drain output)
      int ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
      if (ret >= 0)
      {
        // Got a frame, process and update total samples
        AVFrame* decoded = process_decoded_frame();
        if (decoded) {
            total_samples_decoded_ += decoded->nb_samples;
        }
        return decoded;
      }
      
      // If fully drained (no more frames from codec)
      if (ret == AVERROR_EOF)
      {
        // Update metadata with actual decoded counts at the end
        metadata_.num_samples = total_samples_decoded_;
        if (codec_ctx_->sample_rate > 0) {
            metadata_.duration = static_cast<double>(total_samples_decoded_) / codec_ctx_->sample_rate;
        }
        return nullptr;
      }
      else if (ret < 0 && ret != AVERROR(EAGAIN))
      {
        throw std::runtime_error("Error receiving frame from decoder");
      }

      // 2. Need more input: Read packet from source
      // If input has ended, send NULL packet to drain codec
      if (eof_reached_)
      {
        ret = avcodec_send_packet(codec_ctx_.get(), nullptr);
        if (ret < 0 && ret != AVERROR_EOF)
          throw std::runtime_error("Error sending flush packet");
        continue;
      }

      ret = av_read_frame(fmt_ctx_.get(), packet_.get());
      if (ret < 0)
      {
        if (ret == AVERROR(EAGAIN))
          return nullptr; // No data currently available
        
        if (ret == AVERROR_EOF) {
          eof_reached_ = true;
          continue;
        }
        
        throw std::runtime_error("Error reading frame");
      }

      if (packet_->stream_index != audio_stream_index_)
      {
        av_packet_unref(packet_.get());
        continue;
      }

      ret = avcodec_send_packet(codec_ctx_.get(), packet_.get());
      av_packet_unref(packet_.get());
      if (ret < 0 && ret != AVERROR(EAGAIN))
      {
        throw std::runtime_error("Error sending packet to decoder");
      }
    }
  }

  AudioSamples SingleStreamDecoder::get_all_samples()
  {
    AudioSamples result;
    while (!is_finished())
    {
      auto *f = decode_next();
      if (!f)
        break;

      if (result.data.empty())
      {
        result.sample_rate = f->sample_rate;
        result.data.resize(f->ch_layout.nb_channels);
      }

      for (int c = 0; c < f->ch_layout.nb_channels; ++c)
      {
        const float *channel_data = reinterpret_cast<const float *>(f->data[c]);
        result.data[c].insert(result.data[c].end(), channel_data,
                              channel_data + f->nb_samples);
      }
    }
    return result;
  }

} // namespace avioflow
