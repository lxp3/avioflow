
#include "single-stream-decoder.h"
#include "avio-context-handler.h"
#include "device-handler.h"

namespace avioflow
{

  SingleStreamDecoder::SingleStreamDecoder(
      std::optional<int> output_sample_rate,
      std::optional<int> output_num_channels)
      : packet_(av_packet_alloc()), frame_(av_frame_alloc()),
        converted_frame_(av_frame_alloc()),
        output_sample_rate_(output_sample_rate),
        output_num_channels_(output_num_channels) {}

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

  void SingleStreamDecoder::open(const uint8_t *data, size_t size)
  {
    fmt_ctx_.reset(AvioContextHandler::open_memory(data, size));
    setup_decoder();
  }

  void SingleStreamDecoder::open(const uint8_t *data, size_t size,
                                 int sample_rate, int channels,
                                 const std::string &pcm_format)
  {
    fmt_ctx_.reset(AvioContextHandler::open_memory(data, size, pcm_format,
                                                   sample_rate, channels));
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

    // Populate metadata from codec context (source audio format)
    metadata_.sample_rate = codec_ctx_->sample_rate;
    metadata_.num_channels = codec_ctx_->ch_layout.nb_channels;
    metadata_.duration = static_cast<double>(fmt_ctx_->duration) / AV_TIME_BASE;
    metadata_.sample_format = fmt_ctx_->iformat->name;

    eof_reached_ = false;
    resampler_initialized_ = false;
  }

  void SingleStreamDecoder::setup_resampler(AVFrame *frame)
  {
    // Read actual parameters from the decoded frame (more reliable than codec
    // context)
    int src_sample_rate = frame->sample_rate;
    AVSampleFormat src_sample_format =
        static_cast<AVSampleFormat>(frame->format);
    int src_num_channels = frame->ch_layout.nb_channels;

    // Determine output parameters
    output_sample_rate_ = output_sample_rate_.value_or(src_sample_rate);
    output_num_channels_ = output_num_channels_.value_or(src_num_channels);

    // Check if resampling is actually needed
    needs_resample_ = (src_sample_format != output_sample_format_) ||
                      (src_sample_rate != *output_sample_rate_) ||
                      (src_num_channels != *output_num_channels_);

    if (needs_resample_)
    {
      // Setup output channel layout
      AVChannelLayout out_ch_layout;
      av_channel_layout_default(&out_ch_layout, *output_num_channels_);

      SwrContext *swr = nullptr;
      check_av_error(
          swr_alloc_set_opts2(&swr, &out_ch_layout, output_sample_format_,
                              *output_sample_rate_, &frame->ch_layout,
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
    {
      return src_samples;
    }

    // Use av_rescale_rnd with delay handling for accurate sample count
    int64_t delay = swr_ctx_ ? swr_get_delay(swr_ctx_.get(), src_rate) : 0;
    return static_cast<int>(av_rescale_rnd(
        delay + src_samples, dst_rate, src_rate, AV_ROUND_UP));
  }

  FrameOutput SingleStreamDecoder::decode_next()
  {
    while (av_read_frame(fmt_ctx_.get(), packet_.get()) >= 0)
    {
      if (packet_->stream_index != audio_stream_index_)
      {
        av_packet_unref(packet_.get());
        continue;
      }

      int ret = avcodec_send_packet(codec_ctx_.get(), packet_.get());
      av_packet_unref(packet_.get());
      if (ret < 0)
        continue;

      ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      {
        av_frame_unref(frame_.get());
        continue;
      }
      else if (ret < 0)
      {
        av_frame_unref(frame_.get());
        throw std::runtime_error("Error during decoding");
      }

      // Lazy initialization of resampler on first frame
      if (!resampler_initialized_)
      {
        setup_resampler(frame_.get());
      }

      FrameOutput output = {};

      if (needs_resample_)
      {
        int src_sample_rate = frame_->sample_rate;

        // Calculate output sample count
        int out_samples =
            calculate_output_samples(frame_->nb_samples, src_sample_rate,
                                     *output_sample_rate_);

        // Prepare converted frame
        av_frame_unref(converted_frame_.get());
        converted_frame_->format = output_sample_format_;
        converted_frame_->sample_rate = *output_sample_rate_;
        av_channel_layout_default(&converted_frame_->ch_layout,
                                  *output_num_channels_);
        converted_frame_->nb_samples = out_samples;

        check_av_error(av_frame_get_buffer(converted_frame_.get(), 0),
                       "Could not allocate converted frame buffer");

        // Perform conversion
        int converted = swr_convert(
            swr_ctx_.get(), converted_frame_->data, out_samples,
            const_cast<const uint8_t **>(frame_->extended_data),
            frame_->nb_samples);

        if (converted < 0)
        {
          av_frame_unref(frame_.get());
          throw std::runtime_error("Error during resampling");
        }

        // Update actual sample count after conversion
        converted_frame_->nb_samples = converted;

        output.data = converted_frame_->data[0];
        output.size = static_cast<size_t>(converted) * sizeof(float);
        output.sample_rate = *output_sample_rate_;
        output.num_channels = *output_num_channels_;
        output.num_samples = converted;
      }
      else
      {
        // No resampling needed - return pointer to original frame data
        output.data = frame_->data[0];
        output.size =
            static_cast<size_t>(frame_->nb_samples) * sizeof(float);
        output.sample_rate = frame_->sample_rate;
        output.num_channels = frame_->ch_layout.nb_channels;
        output.num_samples = frame_->nb_samples;
      }

      // Note: We do NOT unref frame_ here to keep data valid until next decode call
      // The frame will be unref'd at the start of the next decode_next() call
      return output;
    }

    eof_reached_ = true;
    return {};
  }

  AudioSamples SingleStreamDecoder::get_all_samples()
  {
    AudioSamples result;

    // Pre-allocate based on estimated duration using av_rescale_rnd
    size_t estimated_samples = 0;
    if (metadata_.duration > 0 && metadata_.sample_rate > 0)
    {
      int out_rate = output_sample_rate_.value_or(metadata_.sample_rate);
      int64_t src_samples =
          static_cast<int64_t>(metadata_.duration * metadata_.sample_rate);

      // Use av_rescale_rnd for consistency, add 10% buffer
      estimated_samples = static_cast<size_t>(av_rescale_rnd(
          src_samples * 11 / 10, out_rate, metadata_.sample_rate, AV_ROUND_UP));
    }

    // Decode all frames
    while (has_more())
    {
      auto frame = decode_next();
      if (frame.data == nullptr)
        break;

      // Initialize result on first frame
      if (result.data.empty())
      {
        result.sample_rate = frame.sample_rate;
        result.data.resize(frame.num_channels);

        if (estimated_samples > 0)
        {
          for (auto &ch : result.data)
          {
            ch.reserve(estimated_samples);
          }
        }
      }

      // Get the source frame for channel data access
      // decode_next() returns data pointer to either frame_ or converted_frame_
      // We need to access all channels from the appropriate source
      AVFrame *src_frame = needs_resample_ ? converted_frame_.get() : frame_.get();

      for (int c = 0; c < frame.num_channels; ++c)
      {
        const float *channel_data =
            reinterpret_cast<const float *>(src_frame->data[c]);

        result.data[c].insert(result.data[c].end(), channel_data,
                              channel_data + frame.num_samples);
      }
    }

    return result;
  }

} // namespace avioflow
