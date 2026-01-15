#include "device-handler.h"
#include "../../include/metadata.h"
#include <vector>

namespace avioflow {

void DeviceHandler::init_devices() {
  static bool initialized = false;
  if (!initialized) {
    avdevice_register_all();
    initialized = true;
  }
}

std::vector<DeviceInfo> DeviceHandler::list_devices() {
  init_devices();
  std::vector<DeviceInfo> devices;

  AVDeviceInfoList *device_list = nullptr;
  const AVInputFormat *ifmt = av_find_input_format("dshow");

  if (ifmt) {
    if (avdevice_list_input_sources(ifmt, nullptr, nullptr, &device_list) >=
        0) {
      for (int i = 0; i < device_list->nb_devices; ++i) {
        AVDeviceInfo *device = device_list->devices[i];
        DeviceInfo info;
        info.name = "audio=" + std::string(device->device_name);
        info.description = device->device_description;
        info.is_output = false;
        devices.push_back(info);
      }
      avdevice_free_list_devices(&device_list);
    }
  }

  // Also check WASAPI for loopback (output) devices
  ifmt = av_find_input_format("wasapi");
  if (ifmt) {
    if (avdevice_list_input_sources(ifmt, nullptr, nullptr, &device_list) >=
        0) {
      for (int i = 0; i < device_list->nb_devices; ++i) {
        AVDeviceInfo *device = device_list->devices[i];
        DeviceInfo info;
        info.name = device->device_name;
        info.description = device->device_description;
        // wasapi devices can be loopback (output)
        if (info.description.find("loopback") != std::string::npos ||
            info.name.find("{0.0.0.00000000}") != std::string::npos) {
          info.is_output = true;
        }
        devices.push_back(info);
      }
      avdevice_free_list_devices(&device_list);
    }
  }

  return devices;
}

AVFormatContext *DeviceHandler::open_device(const std::string &device_name) {
  init_devices();

  const AVInputFormat *ifmt = nullptr;

  // Determine format based on device name or prefix
  if (device_name.find("audio=") == 0 || device_name.find("video=") == 0) {
    ifmt = av_find_input_format("dshow");
  } else if (device_name.find("{") != std::string::npos) {
    // Likely a WASAPI endpoint ID
    ifmt = av_find_input_format("wasapi");
  }

  if (!ifmt) {
    // Fallback
    ifmt = av_find_input_format("dshow");
    if (!ifmt)
      ifmt = av_find_input_format("gdigrab");
  }

  AVFormatContext *fmt_ctx = nullptr;
  AVDictionary *options = nullptr;

  // Set some default options for low latency capture if needed
  // av_dict_set(&options, "rtbufsize", "100M", 0);

  check_av_error(
      avformat_open_input(&fmt_ctx, device_name.c_str(), ifmt, &options),
      "Could not open device " + device_name);

  if (options)
    av_dict_free(&options);

  return fmt_ctx;
}

} // namespace avioflow
