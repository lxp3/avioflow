#pragma once

#include <cstdint>
#include <string>

struct AVFormatContext;

namespace avioflow
{

  class AvioContextHandler
  {
  public:
    // Buffer size for AVIO context (64KB for efficient I/O)
    static constexpr int AVIO_BUFFER_SIZE = 64 * 1024;

    static AVFormatContext *open_url(const std::string &url);
    static AVFormatContext *open_memory(const uint8_t *data, size_t size,
                                        const std::string &format = "",
                                        int sample_rate = 0, int channels = 0);

  private:
    struct MemoryContext
    {
      const uint8_t *data;
      size_t size;
      size_t pos;
    };

    static int read_packet(void *opaque, uint8_t *buf, int buf_size);
    static int64_t seek(void *opaque, int64_t offset, int whence);
  };

} // namespace avioflow
