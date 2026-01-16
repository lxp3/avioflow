#pragma once

#ifdef AVIOFLOW_HAS_WASAPI

#include <vector>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include "miniaudio.h"

namespace avioflow {

class WasapiHandler {
public:
    WasapiHandler();
    ~WasapiHandler();

    void start_capture();
    void stop_capture();

    // Read raw PCM data from the internal buffer
    // Returns number of bytes read, or -1 if no data (EAGAIN)
    int read(uint8_t* buf, int size);

    // Get capture format info
    int get_sample_rate() const { return sample_rate_; }
    int get_num_channels() const { return num_channels_; }
    ma_format get_format() const { return format_; }

private:
    static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

    ma_device device_;
    bool is_running_ = false;
    
    // Internal buffer for captured PCM data
    std::vector<uint8_t> buffer_;
    mutable std::mutex buffer_mutex_;

    int sample_rate_ = 48000;
    int num_channels_ = 2;
    ma_format format_ = ma_format_f32;
};

} // namespace avioflow

#endif // AVIOFLOW_HAS_WASAPI
