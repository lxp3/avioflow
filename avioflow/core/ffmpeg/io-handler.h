
#pragma once

#include "ffmpeg-common.h"
#include <vector>
#include <cstdint>

namespace avioflow {

class IOHandler {
public:
    static AVFormatContext* open_url(const std::string& url);
    static AVFormatContext* open_memory(const uint8_t* data, size_t size);

private:
    struct MemoryContext {
        const uint8_t* data;
        size_t size;
        size_t pos;
    };

    static int read_packet(void* opaque, uint8_t* buf, int buf_size);
    static int64_t seek(void* opaque, int64_t offset, int whence);
};

} // namespace avioflow
