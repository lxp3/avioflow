#pragma once

#include <optional>
#include <string>
#include <vector>
#include "ffmpeg-common.h"
#include "../../include/metadata.h"

struct AVFormatContext;

namespace avioflow
{

  class AvioContextHandler
  {
  public:
    // Buffer size for AVIO context (64KB for efficient I/O)
    static constexpr int AVIO_BUFFER_SIZE = 64 * 1024;

    static AVFormatContext *open_url(const std::string &url);

    // Open using custom I/O callback and opaque pointer
    static AVFormatContext *create_avio_context(
        void *opaque,
        AVIOReadFunction read_packet,
        AVIOSeekFunction seek,
        const AudioStreamOptions &options);

    // Helper for simple memory buffers (wraps create_avio_context)
    static AVFormatContext *open_memory(const uint8_t *data, size_t size,
                                        const AudioStreamOptions &options = {});

    // Helper for callback-based streaming (wraps create_avio_context)
    static AVFormatContext *open_stream(AVIOReadCallback avio_read_callback,
                                        const AudioStreamOptions &options);

  private:
    struct MemoryContext
    {
      const uint8_t *data;
      size_t size;
      size_t pos;
    };

    struct StreamContext
    {
      AVIOReadCallback avio_read_callback;
    };

    static int read_packet_memory(void *opaque, uint8_t *buf, int buf_size);
    static int64_t seek_memory(void *opaque, int64_t offset, int whence);
    static int read_packet_stream(void *opaque, uint8_t *buf, int buf_size);
  };

} // namespace avioflow
