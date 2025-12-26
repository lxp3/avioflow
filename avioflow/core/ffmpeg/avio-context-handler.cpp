
#include "avio-context-handler.h"
#include "ffmpeg-common.h"
#include <algorithm>
#include <cstring>

namespace avioflow {

AVFormatContext *AvioContextHandler::open_url(const std::string &url) {
  AVFormatContext *fmt_ctx = nullptr;
  check_av_error(avformat_open_input(&fmt_ctx, url.c_str(), nullptr, nullptr),
                 "Could not open input " + url);
  return fmt_ctx;
}

AVFormatContext *AvioContextHandler::open_memory(const uint8_t *data,
                                                 size_t size) {
  AVFormatContext *fmt_ctx = avformat_alloc_context();
  if (!fmt_ctx)
    throw std::runtime_error("Could not allocate AVFormatContext");

  uint8_t *avio_ctx_buffer =
      static_cast<uint8_t *>(av_malloc(AVIO_BUFFER_SIZE));
  if (!avio_ctx_buffer)
    throw std::runtime_error("Could not allocate AVIO buffer");

  MemoryContext *m_ctx = new MemoryContext{data, size, 0};

  AVIOContext *avio_ctx = avio_alloc_context(
      avio_ctx_buffer, AVIO_BUFFER_SIZE, 0, m_ctx,
      &AvioContextHandler::read_packet, nullptr, &AvioContextHandler::seek);

  if (!avio_ctx) {
    av_free(avio_ctx_buffer);
    delete m_ctx;
    avformat_free_context(fmt_ctx);
    throw std::runtime_error("Could not allocate AVIOContext");
  }

  // FFmpeg takes ownership of m_ctx in a way, but we need to ensure it's freed.
  // However, avio_context_free only frees the buffer if it was allocated by
  // avio_alloc_context. The opaque data is usually managed by the caller. To
  // simplify RAII, we'll let the user manage life-cycle or wrap it in a custom
  // way. For this implementation, we'll attach it to the AVFormatContext.

  fmt_ctx->pb = avio_ctx;
  fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

  AVDictionary *options = nullptr;
  // Some formats need to be probed
  if (avformat_open_input(&fmt_ctx, nullptr, nullptr, &options) < 0) {
    // Cleaning up here if open_input fails is tricky,
    // SingleStreamDecoder's RAII should handle the ctx eventually.
    throw std::runtime_error("Could not open memory input");
  }

  return fmt_ctx;
}

int AvioContextHandler::read_packet(void *opaque, uint8_t *buf, int buf_size) {
  MemoryContext *m_ctx = static_cast<MemoryContext *>(opaque);
  int read = static_cast<int>(
      std::min(static_cast<size_t>(buf_size), m_ctx->size - m_ctx->pos));
  if (read <= 0)
    return AVERROR_EOF;
  std::memcpy(buf, m_ctx->data + m_ctx->pos, read);
  m_ctx->pos += read;
  return read;
}

int64_t AvioContextHandler::seek(void *opaque, int64_t offset, int whence) {
  MemoryContext *m_ctx = static_cast<MemoryContext *>(opaque);
  if (whence == AVSEEK_SIZE)
    return static_cast<int64_t>(m_ctx->size);

  int64_t new_pos = 0;
  switch (whence) {
  case SEEK_SET:
    new_pos = offset;
    break;
  case SEEK_CUR:
    new_pos = static_cast<int64_t>(m_ctx->pos) + offset;
    break;
  case SEEK_END:
    new_pos = static_cast<int64_t>(m_ctx->size) + offset;
    break;
  default:
    return -1;
  }

  if (new_pos < 0 || static_cast<size_t>(new_pos) > m_ctx->size)
    return -1;
  m_ctx->pos = static_cast<size_t>(new_pos);
  return static_cast<int64_t>(m_ctx->pos);
}

} // namespace avioflow
