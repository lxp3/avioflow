
#include "device-handler.h"

namespace avioflow {

void DeviceHandler::init_devices() {
  static bool initialized = false;
  if (!initialized) {
    avdevice_register_all();
    initialized = true;
  }
}

AVFormatContext *DeviceHandler::open_device(const std::string &device_name) {
  init_devices();

  const AVInputFormat *ifmt =
      av_find_input_format("dshow"); // Windows DirectShow
  if (!ifmt) {
    // Fallback for non-Windows or if dshow is not available in ffmpeg build
    ifmt = av_find_input_format("gdigrab");
  }

  AVFormatContext *fmt_ctx = nullptr;
  AVDictionary *options = nullptr;
  // device_name should be formatted like "audio=Microphone (Realtek Audio)"
  check_av_error(
      avformat_open_input(&fmt_ctx, device_name.c_str(), ifmt, &options),
      "Could not open device " + device_name);

  return fmt_ctx;
}

} // namespace avioflow
