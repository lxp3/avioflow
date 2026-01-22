#include "avio-context-handler.h"
#include "ffmpeg-common.h"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace avioflow
{

  AVFormatContext *AvioContextHandler::open_url(const std::string &url)
  {
    AVFormatContext *fmt_ctx = nullptr;
    check_av_error(avformat_open_input(&fmt_ctx, url.c_str(), nullptr, nullptr),
                   "Could not open input " + url);
    return fmt_ctx;
  }

  AVFormatContext *AvioContextHandler::create_avio_context(
      void *opaque,
      AVIOReadFunction read_packet,
      AVIOSeekFunction seek,
      const AudioStreamOptions &options)
  {
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    if (!fmt_ctx)
      throw std::runtime_error("Could not allocate AVFormatContext");

    uint8_t *avio_ctx_buffer =
        static_cast<uint8_t *>(av_malloc(AVIO_BUFFER_SIZE));
    if (!avio_ctx_buffer)
    {
      avformat_free_context(fmt_ctx);
      throw std::runtime_error("Could not allocate AVIO buffer");
    }

    AVIOContext *avio_ctx = avio_alloc_context(
        avio_ctx_buffer, AVIO_BUFFER_SIZE, 0, opaque,
        read_packet, nullptr, seek);

    if (!avio_ctx)
    {
      av_free(avio_ctx_buffer);
      avformat_free_context(fmt_ctx);
      throw std::runtime_error("Could not allocate AVIOContext");
    }

    // Mark as seekable only if a seek callback is provided
    avio_ctx->seekable = (seek != nullptr) ? AVIO_SEEKABLE_NORMAL : 0;

    fmt_ctx->pb = avio_ctx;
    
    // Allow explicitly specifying input format if auto-detection fails
    const AVInputFormat *iformat = nullptr;
    if (options.input_format.has_value())
    {
      iformat = av_find_input_format(options.input_format->c_str());
    }

    // Set format-specific options if provided (crucial for raw PCM)
    AVDictionary *format_opts = nullptr;
    if (options.input_sample_rate.has_value())
    {
      av_dict_set_int(&format_opts, "sample_rate", options.input_sample_rate.value(), 0);
    }
    if (options.input_channels.has_value())
    {
      av_dict_set_int(&format_opts, "channels", options.input_channels.value(), 0);
    }

    // Attempt to open input using the custom I/O.
    // Passing format_opts allows FFmpeg to know parameters for raw streams.
    int err = avformat_open_input(&fmt_ctx, nullptr, iformat, &format_opts);
    
    // Clean up options (FFmpeg consumes recognized options, but we still need to free the dictionary)
    if (format_opts) {
      av_dict_free(&format_opts);
    }
    if (err < 0)
    {
      char err_buf[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(err, err_buf, sizeof(err_buf));
      std::cerr << "[ERROR] avformat_open_input failed: " << err_buf << " (code: " << err << ")" << std::endl;
      
      av_freep(&avio_ctx->buffer);
      avio_context_free(&avio_ctx);
      avformat_free_context(fmt_ctx);
      throw std::runtime_error("Could not open custom I/O input: " + std::string(err_buf));
    }
    
    return fmt_ctx;
  }

  AVFormatContext *AvioContextHandler::open_memory(const uint8_t *data,
                                                   size_t size,
                                                   const AudioStreamOptions &options)
  {
    MemoryContext *m_ctx = new MemoryContext{data, size, 0};
    try
    {
      return create_avio_context(static_cast<void*>(m_ctx), 
                            AVIOReadFunction(read_packet_memory),
                            AVIOSeekFunction(seek_memory), 
                            options);
    }
    catch (...)
    {
      delete m_ctx;
      throw;
    }
  }

  int AvioContextHandler::read_packet_memory(void *opaque, uint8_t *buf, int buf_size)
  {
    MemoryContext *m_ctx = static_cast<MemoryContext *>(opaque);
    if (m_ctx->pos >= m_ctx->size) {
        return AVERROR_EOF;
    }
    
    size_t available = m_ctx->size - m_ctx->pos;
    int read = std::min(static_cast<int>(available), buf_size);
    
    if (read <= 0)
      return AVERROR_EOF;

    std::memcpy(buf, m_ctx->data + m_ctx->pos, read);
    m_ctx->pos += read;
    return read;
  }

  int64_t AvioContextHandler::seek_memory(void *opaque, int64_t offset, int whence)
  {
    MemoryContext *m_ctx = static_cast<MemoryContext *>(opaque);
    int64_t ret = -1;

    switch (whence)
    {
    case AVSEEK_SIZE:
      ret = static_cast<int64_t>(m_ctx->size);
      break;
    case SEEK_SET:
      m_ctx->pos = static_cast<size_t>(offset);
      ret = offset;
      break;
    default:
      break;  // Return -1 for unsupported operations
    }

    return ret;
  }

  int AvioContextHandler::read_packet_stream(void *opaque, uint8_t *buf, int buf_size)
  {
    StreamContext *s_ctx = static_cast<StreamContext *>(opaque);
    if (s_ctx->avio_read_callback)
    {
      int result = s_ctx->avio_read_callback(buf, buf_size);
      // Translate simple int to FFmpeg error codes:
      // >0: bytes read (pass through)
      // 0: EOF
      // <0: no data available (EAGAIN)
      if (result == 0)
        return AVERROR_EOF;
      if (result < 0)
        return AVERROR(EAGAIN);
      return result;
    }
    return AVERROR_EOF;
  }

  AVFormatContext *AvioContextHandler::open_stream(AVIOReadCallback avio_read_callback,
                                                   const AudioStreamOptions &options)
  {
    StreamContext *s_ctx = new StreamContext{std::move(avio_read_callback)};
    try
    {
      // Streaming input typically doesn't support seeking
      return create_avio_context(static_cast<void*>(s_ctx), 
                            AVIOReadFunction(read_packet_stream),
                            nullptr, 
                            options);
    }
    catch (...)
    {
      delete s_ctx;
      throw;
    }
  }

} // namespace avioflow
