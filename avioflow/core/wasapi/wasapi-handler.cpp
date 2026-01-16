#define MA_IMPLEMENTATION
#define NOMINMAX
#include <windows.h> // Ensure NOMINMAX is applied before any windows header

#pragma warning(push, 0)
#include "miniaudio.h"
#pragma warning(pop)

#include "wasapi-handler.h"
#include <cstring>
#include <stdexcept>
#include <algorithm> // for std::min

namespace avioflow {

void WasapiHandler::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pOutput; // Unreferenced parameter
    auto* handler = static_cast<WasapiHandler*>(pDevice->pUserData);
    if (!pInput || frameCount == 0) return;

    size_t bytesPerFrame = ma_get_bytes_per_frame(handler->format_, handler->num_channels_);
    size_t sizeInBytes = frameCount * bytesPerFrame;

    std::lock_guard<std::mutex> lock(handler->buffer_mutex_);
    const uint8_t* pData = static_cast<const uint8_t*>(pInput);
    
    // Limit buffer size to avoid memory explosion (e.g., 1 second of audio)
    size_t maxBuffer = static_cast<size_t>(handler->sample_rate_) * bytesPerFrame;
    if (handler->buffer_.size() + sizeInBytes > maxBuffer) {
        // Drop oldest data if needed
        size_t overflow = (handler->buffer_.size() + sizeInBytes) - maxBuffer;
        handler->buffer_.erase(handler->buffer_.begin(), handler->buffer_.begin() + (std::min)(overflow, handler->buffer_.size()));
    }
    
    handler->buffer_.insert(handler->buffer_.end(), pData, pData + sizeInBytes);
}

WasapiHandler::WasapiHandler() {
    // For WASAPI loopback, we use ma_device_type_loopback
    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    config.capture.pDeviceID = nullptr; // Default device
    config.capture.format = format_;
    config.capture.channels = num_channels_;
    config.sampleRate = sample_rate_;
    config.dataCallback = data_callback;
    config.pUserData = this;

    if (ma_device_init(nullptr, &config, &device_) != MA_SUCCESS) {
        throw std::runtime_error("Failed to initialize WASAPI loopback device");
    }

    sample_rate_ = device_.sampleRate;
    num_channels_ = device_.capture.channels;
    format_ = device_.capture.format;
}

WasapiHandler::~WasapiHandler() {
    stop_capture();
    ma_device_uninit(&device_);
}

void WasapiHandler::start_capture() {
    if (!is_running_) {
        if (ma_device_start(&device_) == MA_SUCCESS) {
            is_running_ = true;
        } else {
            throw std::runtime_error("Failed to start WASAPI capture");
        }
    }
}

void WasapiHandler::stop_capture() {
    if (is_running_) {
        ma_device_stop(&device_);
        is_running_ = false;
    }
}

int WasapiHandler::read(uint8_t* buf, int size) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    if (buffer_.empty()) return -1; // EAGAIN

    int to_read = static_cast<int>((std::min)(static_cast<size_t>(size), buffer_.size()));
    std::memcpy(buf, buffer_.data(), to_read);
    buffer_.erase(buffer_.begin(), buffer_.begin() + to_read);
    return to_read;
}

} // namespace avioflow


