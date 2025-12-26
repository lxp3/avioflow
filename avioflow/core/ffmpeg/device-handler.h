
#pragma once

#include "ffmpeg-common.h"

namespace avioflow {

class DeviceHandler {
public:
    static void init_devices();
    static AVFormatContext* open_device(const std::string& device_name);
};

} // namespace avioflow
