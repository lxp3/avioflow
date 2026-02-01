#include "ffmpeg-common.h"
#include <algorithm>
#include <cstdlib>
#include <set>

namespace avioflow {

void internal_set_log_level(const char *level) {
  std::string log_level_str;

  if (level == nullptr) {
    const char *env_level = std::getenv("AVIOFLOW_LOG_LEVEL");
    if (env_level != nullptr) {
      log_level_str = env_level;
    } else {
      log_level_str = "info";
    }
  } else {
    log_level_str = level;
  }

  for (auto &c : log_level_str) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  int av_level = AV_LOG_INFO;
  if (log_level_str == "quiet")
    av_level = AV_LOG_QUIET;
  else if (log_level_str == "panic")
    av_level = AV_LOG_PANIC;
  else if (log_level_str == "fatal")
    av_level = AV_LOG_FATAL;
  else if (log_level_str == "error")
    av_level = AV_LOG_ERROR;
  else if (log_level_str == "warning")
    av_level = AV_LOG_WARNING;
  else if (log_level_str == "info")
    av_level = AV_LOG_INFO;
  else if (log_level_str == "verbose")
    av_level = AV_LOG_VERBOSE;
  else if (log_level_str == "debug")
    av_level = AV_LOG_DEBUG;
  else if (log_level_str == "trace")
    av_level = AV_LOG_TRACE;

  av_log_set_level(av_level);
}

std::vector<std::string> internal_get_supported_decoders() {
  std::set<std::string> decoders;
  const AVCodec *codec = nullptr;
  void *i = nullptr;
  while ((codec = av_codec_iterate(&i))) {
    if (av_codec_is_decoder(codec) && codec->type == AVMEDIA_TYPE_AUDIO) {
      decoders.insert(codec->name);
    }
  }
  return std::vector<std::string>(decoders.begin(), decoders.end());
}

std::vector<std::string> internal_get_supported_encoders() {
  std::set<std::string> encoders;
  const AVCodec *codec = nullptr;
  void *i = nullptr;
  while ((codec = av_codec_iterate(&i))) {
    if (av_codec_is_encoder(codec) && codec->type == AVMEDIA_TYPE_AUDIO) {
      encoders.insert(codec->name);
    }
  }
  return std::vector<std::string>(encoders.begin(), encoders.end());
}

std::vector<std::string> internal_get_supported_input_formats() {
  std::set<std::string> formats;
  const AVInputFormat *iformat = nullptr;
  void *i = nullptr;
  while ((iformat = av_demuxer_iterate(&i))) {
    if (iformat->name) {
      formats.insert(iformat->name);
    }
  }
  return std::vector<std::string>(formats.begin(), formats.end());
}

} // namespace avioflow
