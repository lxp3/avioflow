#include "single-stream-decoder.h"
#include "avio-context-handler.h"
#include "device-handler.h"
#include <mutex>
#include <functional>
#include <cstring>

namespace avioflow
{

  SingleStreamDecoder::SingleStreamDecoder(const AudioStreamOptions &options)
      : packet_(av_packet_alloc()), frame_(av_frame_alloc()),
        converted_frame_(av_frame_alloc()), options_(options) {}

  void SingleStreamDecoder::open(const std::string &source)
  {
    if (mode_ != Mode::None)
      throw std::runtime_error("Decoder already initialized");
    mode_ = Mode::File;

#ifdef AVIOFLOW_HAS_WASAPI
    if (source == "wasapi_loopback")
    {
      is_wasapi_mode_ = true;
      wasapi_handler_ = std::make_unique<WasapiHandler>();
      
      metadata_.sample_rate = wasapi_handler_->get_sample_rate();
      metadata_.num_channels = wasapi_handler_->get_num_channels();
      metadata_.codec = "pcm_f32le";
      metadata_.container = "wasapi_loopback";
      metadata_.sample_format = "f32";
      metadata_.duration = 0.0;
      metadata_.num_samples = 0;
      
      wasapi_handler_->start_capture();
      return;
    }
#endif

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
    if (mode_ != Mode::None)
      throw std::runtime_error("Decoder already initialized");
    mode_ = Mode::File;

    fmt_ctx_.reset(AvioContextHandler::open_memory(data, size, options_));
    setup_decoder();
  }

  void SingleStreamDecoder::push(const uint8_t *data, size_t size)
  {
    if (mode_ == Mode::File)
    {
      throw std::runtime_error("Cannot push data: decoder opened in file mode");
    }

    // Add data to buffer
    {
      std::lock_guard<std::mutex> lock(buffer_mtx_);
      push_buffer_.insert(push_buffer_.end(), data, data + size);
    }

    // Mark as stream mode, but delay initialization until decode_next
    if (mode_ == Mode::None)
    {
      mode_ = Mode::Stream;
    }
  }

  void SingleStreamDecoder::init_stream_context()
  {
    if (!options_.input_format.has_value())
    {
      throw std::runtime_error("input_format must be specified for streaming mode");
    }

    // Special handling for WAV to PCM fallback
    if (options_.input_format.value() == "wav")
    {
      std::lock_guard<std::mutex> lock(buffer_mtx_);
      bool has_wav_header = false;
      if (push_buffer_.size() >= 12)
      {
        if (std::memcmp(push_buffer_.data(), "RIFF", 4) == 0 &&
            std::memcmp(push_buffer_.data() + 8, "WAVE", 4) == 0)
        {
          has_wav_header = true;
        }
      }

      if (!has_wav_header)
      {
        // Fallback to PCM s16le if no WAV header found
        options_.input_format = "s16le";
      }
    }

    // Create AVIO context that reads from our internal push_buffer_
    // Note: open_stream internally calls avformat_open_input with options_.input_format
    // probesize/max_analyze_duration are set in create_avio_context before avformat_open_input
    fmt_ctx_.reset(AvioContextHandler::open_stream([this](uint8_t *buf, int buf_size)
                                                   {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        if (push_buffer_.empty()) return -1; // AVERROR(EAGAIN)
        int read = std::min(static_cast<int>(push_buffer_.size()), buf_size);
        std::memcpy(buf, push_buffer_.data(), read);
        push_buffer_.erase(push_buffer_.begin(), push_buffer_.begin() + read);
        return read; },
                                                   options_));

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

    metadata_.sample_rate = codec_ctx_->sample_rate;
    metadata_.num_channels = codec_ctx_->ch_layout.nb_channels;
    metadata_.codec = codec->name;
    metadata_.bit_rate = fmt_ctx_->bit_rate > 0 ? fmt_ctx_->bit_rate : stream->codecpar->bit_rate;
    metadata_.container = fmt_ctx_->iformat->name;
    metadata_.sample_format = av_get_sample_fmt_name(codec_ctx_->sample_fmt);

    if (stream->duration > 0 && stream->time_base.den > 0) {
        metadata_.duration = static_cast<double>(stream->duration) * av_q2d(stream->time_base);
    } else if (fmt_ctx_->duration != AV_NOPTS_VALUE && fmt_ctx_->duration > 0) {
        metadata_.duration = static_cast<double>(fmt_ctx_->duration) / AV_TIME_BASE;
    } else {
        metadata_.duration = 0.0;
    }

    if (metadata_.duration > 0 && metadata_.sample_rate > 0) {
        metadata_.num_samples = static_cast<int64_t>(metadata_.duration * metadata_.sample_rate);
    }

    total_samples_decoded_ = 0;
    eof_reached_ = false;
    resampler_initialized_ = false;
  }

  void SingleStreamDecoder::setup_resampler(AVFrame *frame)
  {
    int src_sample_rate = frame->sample_rate;
    AVSampleFormat src_sample_format = static_cast<AVSampleFormat>(frame->format);
    int out_rate = src_sample_rate;
    if (options_.output_sample_rate.has_value() &&
        options_.output_sample_rate.value() >= 0) {
      out_rate = options_.output_sample_rate.value();
    }
    AVChannelLayout out_layout;
    if (options_.output_num_channels.has_value() &&
        options_.output_num_channels.value() > 0) {
      av_channel_layout_default(&out_layout, options_.output_num_channels.value());
    } else {
      check_av_error(av_channel_layout_copy(&out_layout, &frame->ch_layout),
                     "Could not copy output channel layout");
    }

    needs_resample_ = (src_sample_format != output_sample_format_) ||
                      (src_sample_rate != out_rate) ||
                      (av_channel_layout_compare(&frame->ch_layout, &out_layout) != 0);

    if (needs_resample_)
    {
      SwrContext *swr = nullptr;
      check_av_error(
          swr_alloc_set_opts2(&swr, &out_layout, output_sample_format_,
                              out_rate, &frame->ch_layout,
                              src_sample_format, src_sample_rate, 0, nullptr),
          "Could not initialize resampler");

      swr_ctx_.reset(swr);
      check_av_error(swr_init(swr_ctx_.get()),
                     "Could not initialize resampler context");
    }

    metadata_.sample_rate = out_rate;
    metadata_.num_channels = out_layout.nb_channels;
    av_channel_layout_uninit(&out_layout);
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
      int out_rate = frame_->sample_rate;
      if (options_.output_sample_rate.has_value() &&
          options_.output_sample_rate.value() >= 0) {
        out_rate = options_.output_sample_rate.value();
      }
      int out_samples = calculate_output_samples(frame_->nb_samples, frame_->sample_rate, out_rate);

      av_frame_unref(converted_frame_.get());
      converted_frame_->format = output_sample_format_;
      converted_frame_->sample_rate = out_rate;
      if (options_.output_num_channels.has_value() &&
          options_.output_num_channels.value() > 0) {
        av_channel_layout_default(&converted_frame_->ch_layout,
                                  options_.output_num_channels.value());
      } else {
        check_av_error(av_channel_layout_copy(&converted_frame_->ch_layout, &frame_->ch_layout),
                       "Could not copy channel layout");
      }
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
      metadata_.sample_rate = frame_->sample_rate;
      metadata_.num_channels = frame_->ch_layout.nb_channels;
      return frame_.get();
    }
  }

  AVFrame *SingleStreamDecoder::read()
  {
    // Lazy initialization for stream mode - wait until we have enough data
    if (mode_ == Mode::Stream && !fmt_ctx_)
    {
      size_t buffer_size;
      {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        buffer_size = push_buffer_.size();
      }
      // Need at least 32KB for format probing (matches probesize setting)
      if (buffer_size < 32 * 1024)
      {
        return nullptr; // Not enough data yet, caller should push more
      }
      init_stream_context();
    }

#ifdef AVIOFLOW_HAS_WASAPI
    if (is_wasapi_mode_)
    {
      const int target_frames = 512;
      int bytes_per_sample = 4;
      int channels = wasapi_handler_->get_num_channels();
      int buf_size = target_frames * channels * bytes_per_sample;
      
      std::vector<uint8_t> tmp_buf(buf_size);
      int read_bytes = wasapi_handler_->read(tmp_buf.data(), buf_size);
      
      if (read_bytes <= 0) return nullptr;

      int read_frames = read_bytes / (channels * bytes_per_sample);
      
      av_frame_unref(frame_.get());
      frame_->format = AV_SAMPLE_FMT_FLT;
      frame_->sample_rate = wasapi_handler_->get_sample_rate();
      av_channel_layout_default(&frame_->ch_layout, channels);
      frame_->nb_samples = read_frames;
      
      check_av_error(av_frame_get_buffer(frame_.get(), 0), "Could not allocate frame buffer");
      std::memcpy(frame_->data[0], tmp_buf.data(), read_bytes);
      
      AVFrame* decoded = process_decoded_frame();
      if (decoded) {
          total_samples_decoded_ += decoded->nb_samples;
          metadata_.num_samples = total_samples_decoded_;
      }
      return decoded;
    }
#endif

    while (true)
    {
      int ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
      if (ret >= 0)
      {
        AVFrame* decoded = process_decoded_frame();
        if (decoded) {
            total_samples_decoded_ += decoded->nb_samples;
        }
        return decoded;
      }
      
      if (ret == AVERROR_EOF)
      {
        metadata_.num_samples = total_samples_decoded_;
        if (metadata_.sample_rate > 0) {
            metadata_.duration = static_cast<double>(total_samples_decoded_) / metadata_.sample_rate;
        }
        eof_reached_ = true;
        return nullptr;
      }
      else if (ret < 0 && ret != AVERROR(EAGAIN))
      {
        throw std::runtime_error("Error receiving frame from decoder");
      }

      if (eof_reached_)
      {
        // Flush the decoder
        ret = avcodec_send_packet(codec_ctx_.get(), nullptr);
        if (ret < 0 && ret != AVERROR_EOF)
          throw std::runtime_error("Error sending flush packet");
        
        // Try to receive one last time after flush
        ret = avcodec_receive_frame(codec_ctx_.get(), frame_.get());
        if (ret >= 0) {
            AVFrame* decoded = process_decoded_frame();
            if (decoded) total_samples_decoded_ += decoded->nb_samples;
            return decoded;
        }
        return nullptr; // Truly finished
      }

      ret = av_read_frame(fmt_ctx_.get(), packet_.get());
      if (ret < 0)
      {
        if (ret == AVERROR(EAGAIN))
          return nullptr;

        if (ret == AVERROR_EOF)
        {
          eof_reached_ = true;
          // Send flush packet to decoder immediately when input EOF is reached
          avcodec_send_packet(codec_ctx_.get(), nullptr);
          continue;
        }

        // Strict error handling for stream mode
        throw std::runtime_error("Error reading frame: " + std::to_string(ret));
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

  std::vector<std::vector<float>> SingleStreamDecoder::get_samples()
  {
    std::vector<std::vector<float>> result;
    while (true)
    {
      auto *f = read();
      if (!f)
        break;

      if (result.empty())
      {
        result.resize(f->ch_layout.nb_channels);
      }

      for (int c = 0; c < f->ch_layout.nb_channels; ++c)
      {
        const float *channel_data = reinterpret_cast<const float *>(f->data[c]);
        result[c].insert(result[c].end(), channel_data,
                              channel_data + f->nb_samples);
      }
    }
    return result;
  }

} // namespace avioflow
