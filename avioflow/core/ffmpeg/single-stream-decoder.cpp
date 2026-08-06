#include "single-stream-decoder.h"
#include "avio-context-handler.h"
#include "device-handler.h"
#include <mutex>
#include <functional>
#include <cstring>
#include <cmath>
#include <limits>
#include <new>

namespace avioflow
{
  namespace
  {
    bool is_raw_pcm_input_format(const std::optional<std::string> &format)
    {
      if (!format.has_value())
        return false;

      const std::string &value = format.value();
      return value == "s8" || value == "u8" ||
             value == "s16le" || value == "s16be" ||
             value == "u16le" || value == "u16be" ||
             value == "s24le" || value == "s24be" ||
             value == "u24le" || value == "u24be" ||
             value == "s32le" || value == "s32be" ||
             value == "u32le" || value == "u32be" ||
             value == "f32le" || value == "f32be" ||
             value == "f64le" || value == "f64be";
    }

    int raw_pcm_bytes_per_sample(const std::string &format)
    {
      if (format == "s8" || format == "u8")
        return 1;
      if (format == "s16le" || format == "s16be" ||
          format == "u16le" || format == "u16be")
        return 2;
      if (format == "s24le" || format == "s24be" ||
          format == "u24le" || format == "u24be")
        return 3;
      if (format == "s32le" || format == "s32be" ||
          format == "u32le" || format == "u32be" ||
          format == "f32le" || format == "f32be")
        return 4;
      if (format == "f64le" || format == "f64be")
        return 8;
      return 0;
    }

    uint64_t read_uint(const uint8_t *data, int bytes, bool big_endian)
    {
      uint64_t value = 0;
      for (int i = 0; i < bytes; ++i)
      {
        int idx = big_endian ? i : bytes - 1 - i;
        value = (value << 8) | data[idx];
      }
      return value;
    }

    int64_t sign_extend(uint64_t value, int bits)
    {
      const uint64_t sign_bit = 1ULL << (bits - 1);
      return static_cast<int64_t>((value ^ sign_bit) - sign_bit);
    }

    float raw_pcm_to_float(const uint8_t *data, const std::string &format)
    {
      const bool big = format.size() >= 2 && format.substr(format.size() - 2) == "be";
      if (format == "s8")
        return std::max(-1.0f, static_cast<int8_t>(data[0]) / 128.0f);
      if (format == "u8")
        return (static_cast<int>(data[0]) - 128) / 128.0f;
      if (format.rfind("f32", 0) == 0)
      {
        uint32_t bits = static_cast<uint32_t>(read_uint(data, 4, big));
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
      }
      if (format.rfind("f64", 0) == 0)
      {
        uint64_t bits = read_uint(data, 8, big);
        double value;
        std::memcpy(&value, &bits, sizeof(value));
        return static_cast<float>(value);
      }

      const bool is_unsigned = !format.empty() && format[0] == 'u';
      const int bytes = raw_pcm_bytes_per_sample(format);
      const int bits = bytes * 8;
      uint64_t raw = read_uint(data, bytes, big);
      int64_t signed_value = is_unsigned
                                 ? static_cast<int64_t>(raw) - static_cast<int64_t>(1ULL << (bits - 1))
                                 : sign_extend(raw, bits);
      return static_cast<float>(signed_value) / static_cast<float>(1ULL << (bits - 1));
    }
  }

  SingleStreamDecoder::SingleStreamDecoder(const AudioStreamOptions &options)
      : packet_(av_packet_alloc()), frame_(av_frame_alloc()),
        converted_frame_(av_frame_alloc()), options_(options) {
    validate_options(options_);
    if (!packet_ || !frame_ || !converted_frame_) {
      throw std::bad_alloc();
    }
  }

  void SingleStreamDecoder::validate_options(const AudioStreamOptions &options)
  {
    if (options.output_sample_rate &&
        (*options.output_sample_rate == 0 || *options.output_sample_rate < -1))
      throw std::invalid_argument(
          "output_sample_rate must be -1 or greater than zero");
    if (options.output_num_channels &&
        (*options.output_num_channels == 0 || *options.output_num_channels < -1))
      throw std::invalid_argument(
          "output_num_channels must be -1 or greater than zero");
    if (options.input_sample_rate && *options.input_sample_rate <= 0)
      throw std::invalid_argument("input_sample_rate must be greater than zero");
    if (options.input_channels && *options.input_channels <= 0)
      throw std::invalid_argument("input_channels must be greater than zero");
    if (options.input_format && options.input_format->empty())
      throw std::invalid_argument("input_format must not be empty");
  }

  void SingleStreamDecoder::load_file(const std::string &source)
  {
    if (mode_ != Mode::None)
      throw std::runtime_error("Decoder already initialized");
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
      mode_ = Mode::Offline;
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
    mode_ = Mode::Offline;
  }

  void SingleStreamDecoder::load_buffer(const uint8_t *data, size_t size)
  {
    if (mode_ != Mode::None)
      throw std::runtime_error("Decoder already initialized");
    if (!data && size != 0)
      throw std::invalid_argument("Buffer data must not be null when size is non-zero");
    fmt_ctx_.reset(AvioContextHandler::open_memory(data, size, options_));
    setup_decoder();
    mode_ = Mode::Offline;
  }

  void SingleStreamDecoder::feed(const uint8_t *data, size_t size)
  {
    if (mode_ == Mode::Offline)
    {
      throw std::runtime_error("Cannot feed data: decoder loaded in offline mode");
    }
    if (input_finished_)
    {
      throw std::runtime_error("Cannot feed data: stream input already flushed");
    }

    if (!data && size != 0)
      throw std::invalid_argument("Feed data must not be null when size is non-zero");
    if (size == 0)
      return;

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

  void SingleStreamDecoder::flush()
  {
    if (mode_ == Mode::Offline)
    {
      throw std::runtime_error("Cannot flush stream: decoder loaded in offline mode");
    }

    {
      std::lock_guard<std::mutex> lock(buffer_mtx_);
      input_finished_ = true;
      if (mode_ == Mode::None)
      {
        eof_reached_ = true;
      }
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
    try
    {
      fmt_ctx_.reset(AvioContextHandler::open_stream([this](uint8_t *buf, int buf_size)
                                                     {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        if (stream_read_offset_ >= push_buffer_.size()) return input_finished_ ? 0 : -1;
        const size_t requested = buf_size > 0 ? static_cast<size_t>(buf_size) : 0;
        const size_t read_size = std::min(
            push_buffer_.size() - stream_read_offset_, requested);
        const int read = static_cast<int>(read_size);
        std::memcpy(buf, push_buffer_.data() + stream_read_offset_, read_size);
        stream_read_offset_ += read_size;
        return read; },
                                                     options_));
    }
    catch (...)
    {
      std::lock_guard<std::mutex> lock(buffer_mtx_);
      stream_read_offset_ = 0;
      throw;
    }

    setup_decoder();
    {
      std::lock_guard<std::mutex> lock(buffer_mtx_);
      if (stream_read_offset_ > 0)
      {
        push_buffer_.erase(push_buffer_.begin(), push_buffer_.begin() + static_cast<std::ptrdiff_t>(stream_read_offset_));
        stream_read_offset_ = 0;
      }
    }
  }

  void SingleStreamDecoder::setup_decoder()
  {
    if (!(mode_ == Mode::Stream && is_raw_pcm_input_format(options_.input_format)))
    {
      check_av_error(avformat_find_stream_info(fmt_ctx_.get(), nullptr),
                     "Could not find stream info");
    }

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
    resampler_drained_ = false;
  }

  void SingleStreamDecoder::seek_to(double start_seconds)
  {
    resampler_drained_ = false;
    if (start_seconds <= 0.0)
      return;
    const int64_t ts = static_cast<int64_t>(start_seconds * AV_TIME_BASE);
    check_av_error(av_seek_frame(fmt_ctx_.get(), -1, ts, AVSEEK_FLAG_BACKWARD),
                   "Could not seek to start_seconds");
    avcodec_flush_buffers(codec_ctx_.get());
  }

  void SingleStreamDecoder::setup_resampler(AVFrame *frame)
  {
    int src_sample_rate = frame->sample_rate;
    AVSampleFormat src_sample_format = static_cast<AVSampleFormat>(frame->format);
    int out_rate = src_sample_rate;
    if (options_.output_sample_rate.has_value() &&
        options_.output_sample_rate.value() > 0) {
      out_rate = options_.output_sample_rate.value();
    }
    AVChannelLayout out_layout{};
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
          options_.output_sample_rate.value() > 0) {
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

  // Drains samples still buffered inside swresample at end of stream. Without
  // this the resampler's internal delay (a few dozen samples at typical rate
  // ratios) is silently dropped from the tail of the decoded output.
  AVFrame *SingleStreamDecoder::drain_resampler()
  {
    if (resampler_drained_ || !needs_resample_ || !swr_ctx_)
      return nullptr;
    resampler_drained_ = true;

    const int out_rate = metadata_.sample_rate > 0 ? metadata_.sample_rate
                                                   : converted_frame_->sample_rate;
    if (out_rate <= 0)
      return nullptr;

    const int64_t delay = swr_get_delay(swr_ctx_.get(), out_rate);
    if (delay <= 0)
      return nullptr;

    const int out_samples = static_cast<int>(delay);
    const int out_channels = converted_frame_->ch_layout.nb_channels > 0
                                 ? converted_frame_->ch_layout.nb_channels
                                 : metadata_.num_channels;
    if (out_channels <= 0)
      return nullptr;

    av_frame_unref(converted_frame_.get());
    converted_frame_->format = output_sample_format_;
    converted_frame_->sample_rate = out_rate;
    av_channel_layout_default(&converted_frame_->ch_layout, out_channels);
    converted_frame_->nb_samples = out_samples;
    check_av_error(av_frame_get_buffer(converted_frame_.get(), 0),
                   "Could not allocate resampler drain buffer");

    const int converted = swr_convert(swr_ctx_.get(), converted_frame_->data,
                                      out_samples, nullptr, 0);
    if (converted <= 0)
      return nullptr;

    converted_frame_->nb_samples = converted;
    return converted_frame_.get();
  }

  void SingleStreamDecoder::update_decoded_metadata(const AVFrame *frame)
  {
    if (!frame)
      return;

    total_samples_decoded_ += frame->nb_samples;
    metadata_.num_samples = total_samples_decoded_;
    if (metadata_.sample_rate > 0)
    {
      metadata_.duration =
          static_cast<double>(total_samples_decoded_) / metadata_.sample_rate;
    }
  }

  // Trims `frame` in place to the [start_seconds, stop_seconds) range, at sample
  // accuracy. Must be called immediately after update_decoded_metadata() so
  // total_samples_decoded_ reflects samples through the end of `frame`.
  SingleStreamDecoder::TrimAction SingleStreamDecoder::trim_to_range(
      AVFrame *frame, double start_seconds, const std::optional<double> &stop_seconds)
  {
    if (metadata_.sample_rate <= 0)
      return TrimAction::Keep;

    const int64_t frame_start = total_samples_decoded_ - frame->nb_samples;

    if (start_seconds > 0.0)
    {
      const int64_t skip_until = static_cast<int64_t>(
          std::llround(start_seconds * metadata_.sample_rate));
      if (total_samples_decoded_ <= skip_until)
        return TrimAction::Skip; // frame entirely before start_seconds

      if (frame_start < skip_until)
      {
        const int trim = static_cast<int>(skip_until - frame_start);
        for (int c = 0; c < frame->ch_layout.nb_channels; ++c)
          frame->data[c] += trim * static_cast<int>(sizeof(float));
        frame->nb_samples -= trim;
      }
    }

    if (stop_seconds)
    {
      const int64_t stop_at = static_cast<int64_t>(
          std::llround(stop_seconds.value() * metadata_.sample_rate));
      if (frame_start >= stop_at)
        return TrimAction::Stop; // fully past stop_seconds

      if (total_samples_decoded_ > stop_at)
      {
        frame->nb_samples = static_cast<int>(stop_at - frame_start);
        if (frame->nb_samples <= 0)
          return TrimAction::Stop;
      }
    }

    return TrimAction::Keep;
  }

  AVFrame *SingleStreamDecoder::read_raw_pcm_frame()
  {
    if (!options_.input_format.has_value() ||
        !options_.input_sample_rate.has_value() ||
        !options_.input_channels.has_value())
    {
      throw std::runtime_error("input_format, input_sample_rate, and input_channels are required for raw PCM streaming");
    }

    const std::string format = options_.input_format.value();
    const int bytes_per_sample = raw_pcm_bytes_per_sample(format);
    const int channels = options_.input_channels.value();
    if (bytes_per_sample <= 0 || channels <= 0 || options_.input_sample_rate.value() <= 0)
      throw std::runtime_error("Invalid raw PCM stream options");

    std::vector<uint8_t> chunk;
    int samples = 0;
    {
      std::lock_guard<std::mutex> lock(buffer_mtx_);
      const size_t bytes_per_frame = static_cast<size_t>(bytes_per_sample * channels);
      const size_t available_samples = push_buffer_.size() / bytes_per_frame;
      samples = static_cast<int>(std::min(
          available_samples, static_cast<size_t>(std::numeric_limits<int>::max())));
      if (samples <= 0)
      {
        if (input_finished_)
        {
          push_buffer_.clear();
          eof_reached_ = true;
        }
        else
        {
          return nullptr;
        }
      }
      if (samples <= 0)
      {
        // Raw PCM input is exhausted: hand back the resampler tail, if any.
        AVFrame *tail = drain_resampler();
        if (tail)
          update_decoded_metadata(tail);
        return tail;
      }

      const size_t bytes_to_read = static_cast<size_t>(samples) * bytes_per_frame;
      chunk.assign(push_buffer_.begin(), push_buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_to_read));
      push_buffer_.erase(push_buffer_.begin(), push_buffer_.begin() + static_cast<std::ptrdiff_t>(bytes_to_read));
    }

    av_frame_unref(frame_.get());
    frame_->format = AV_SAMPLE_FMT_FLTP;
    frame_->sample_rate = options_.input_sample_rate.value();
    av_channel_layout_default(&frame_->ch_layout, channels);
    frame_->nb_samples = samples;
    check_av_error(av_frame_get_buffer(frame_.get(), 0), "Could not allocate raw PCM frame buffer");

    const size_t bytes_per_frame = static_cast<size_t>(bytes_per_sample * channels);
    for (int i = 0; i < samples; ++i)
    {
      const uint8_t *base = chunk.data() + static_cast<size_t>(i) * bytes_per_frame;
      for (int c = 0; c < channels; ++c)
      {
        reinterpret_cast<float *>(frame_->data[c])[i] =
            raw_pcm_to_float(base + static_cast<size_t>(c * bytes_per_sample), format);
      }
    }

    metadata_.codec = "pcm_" + format;
    metadata_.container = format;
    metadata_.sample_format = "fltp";
    AVFrame *decoded = process_decoded_frame();
    if (decoded)
      update_decoded_metadata(decoded);
    return decoded;
  }

  AVFrame *SingleStreamDecoder::get_frame() { return decode_next_frame(); }

  AVFrame *SingleStreamDecoder::decode_next_frame()
  {
    if (mode_ == Mode::Stream && is_raw_pcm_input_format(options_.input_format))
    {
      return read_raw_pcm_frame();
    }

    // Lazy initialization for stream mode - wait until we have enough data
    if (mode_ == Mode::Stream && !fmt_ctx_)
    {
      size_t buffer_size;
      {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        buffer_size = push_buffer_.size();
      }
      const size_t required_buffer_size =
          is_raw_pcm_input_format(options_.input_format) ? 1 : 32 * 1024;
      bool input_finished;
      {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        input_finished = input_finished_;
      }
      if (buffer_size < required_buffer_size && !input_finished)
      {
        return nullptr; // Not enough data yet, caller should push more
      }
      try
      {
        init_stream_context();
      }
      catch (const std::runtime_error &)
      {
        if (!input_finished)
        {
          stream_read_offset_ = 0;
          return nullptr;
        }
        throw;
      }
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
          update_decoded_metadata(decoded);
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
            update_decoded_metadata(decoded);
        }
        return decoded;
      }

      if (ret == AVERROR_EOF)
      {
        eof_reached_ = true;
        if (AVFrame *tail = drain_resampler())
        {
          update_decoded_metadata(tail);
          return tail;
        }
        metadata_.num_samples = total_samples_decoded_;
        if (metadata_.sample_rate > 0) {
            metadata_.duration = static_cast<double>(total_samples_decoded_) / metadata_.sample_rate;
        }
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
            if (decoded) update_decoded_metadata(decoded);
            return decoded;
        }
        if (AVFrame *tail = drain_resampler())
        {
          update_decoded_metadata(tail);
          return tail;
        }
        return nullptr; // Truly finished
      }

      if (mode_ == Mode::Stream)
      {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        if (push_buffer_.empty() && !input_finished_)
        {
          return nullptr;
        }
      }

      ret = av_read_frame(fmt_ctx_.get(), packet_.get());
      if (mode_ == Mode::Stream && stream_read_offset_ > 0)
      {
        std::lock_guard<std::mutex> lock(buffer_mtx_);
        if (stream_read_offset_ > 0)
        {
          push_buffer_.erase(push_buffer_.begin(), push_buffer_.begin() + static_cast<std::ptrdiff_t>(stream_read_offset_));
          stream_read_offset_ = 0;
        }
      }
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

  std::vector<std::vector<float>> SingleStreamDecoder::get_samples(
      double start_seconds, std::optional<double> stop_seconds)
  {
    if (start_seconds < 0.0)
      throw std::invalid_argument("start_seconds must be >= 0");
    if (stop_seconds && *stop_seconds <= 0.0)
      throw std::invalid_argument("stop_seconds must be greater than zero");
    if (stop_seconds && *stop_seconds <= start_seconds)
      throw std::invalid_argument("stop_seconds must be greater than start_seconds");

    const bool has_range = start_seconds > 0.0 || stop_seconds.has_value();
    if (has_range && mode_ != Mode::Offline)
      throw std::runtime_error("start_seconds/stop_seconds require offline mode");

    if (has_range)
    {
      seek_to(start_seconds);
      total_samples_decoded_ = 0;
      eof_reached_ = false;
    }

    std::vector<std::vector<float>> result;
    while (true)
    {
      AVFrame *f = decode_next_frame();
      if (!f)
        break;

      if (has_range && mode_ == Mode::Offline)
      {
        const TrimAction action = trim_to_range(f, start_seconds, stop_seconds);
        if (action == TrimAction::Skip)
          continue;
        if (action == TrimAction::Stop)
        {
          eof_reached_ = true;
          break;
        }
      }

      if (result.empty())
      {
        result.resize(static_cast<size_t>(f->ch_layout.nb_channels));
      }

      for (int c = 0; c < f->ch_layout.nb_channels; ++c)
      {
        const float *channel_data = reinterpret_cast<const float *>(f->data[c]);
        const size_t channel = static_cast<size_t>(c);
        result[channel].insert(result[channel].end(), channel_data,
                              channel_data + f->nb_samples);
      }
    }
    return result;
  }

} // namespace avioflow
