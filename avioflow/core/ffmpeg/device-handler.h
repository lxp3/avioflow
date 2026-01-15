
#pragma once

#include "../../include/metadata.h"
#include "ffmpeg-common.h"
#include <vector>


namespace avioflow {

class DeviceHandler {
public:
  static void init_devices();
  static std::vector<DeviceInfo> list_devices();
  static AVFormatContext *open_device(const std::string &device_name);
};

} // namespace avioflow
