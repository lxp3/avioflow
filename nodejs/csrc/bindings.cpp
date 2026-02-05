/**
 * @file bindings.cpp
 * @brief Node.js bindings for avioflow audio decoding library using
 * node-addon-api.
 *
 * Provides high-performance audio decoding capabilities to Node.js.
 * Audio data is returned as Float32Array per channel.
 */

#include "avioflow-cxx-api.h"
#include <napi.h>
#include <string>
#include <vector>


using namespace avioflow;

// --- Helper Functions ---

/**
 * @brief Helper to convert Metadata to Napi::Object
 */
Napi::Object MetadataToJs(Napi::Env env, const Metadata &meta) {
  Napi::Object obj = Napi::Object::New(env);
  obj.Set("duration", meta.duration);
  obj.Set("sampleRate", meta.sample_rate);
  obj.Set("numChannels", meta.num_channels);
  obj.Set("codec", meta.codec);
  obj.Set("numSamples", meta.num_samples);
  obj.Set("sampleFormat", meta.sample_format);
  obj.Set("bitRate", meta.bit_rate);
  obj.Set("container", meta.container);
  return obj;
}

/**
 * @brief Helper to convert samples vector to Napi::Array of Float32Array
 */
Napi::Array SamplesToJs(Napi::Env env,
                        const std::vector<std::vector<float>> &samples) {
  Napi::Array channelsArr = Napi::Array::New(env, samples.size());
  for (size_t c = 0; c < samples.size(); ++c) {
    Napi::Float32Array data = Napi::Float32Array::New(env, samples[c].size());
    std::copy(samples[c].begin(), samples[c].end(), data.Data());
    channelsArr[c] = data;
  }
  return channelsArr;
}

// --- Module-level functions ---

/**
 * @brief Set FFmpeg logging verbosity level.
 *
 * @param info Callback info containing:
 *   - level (string): "quiet", "fatal", "error", "warning", "info", "debug",
 * "trace"
 */
void SetLogLevel(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "String expected for log level")
        .ThrowAsJavaScriptException();
    return;
  }
  std::string level = info[0].As<Napi::String>().Utf8Value();
  avioflow_set_log_level(level.c_str());
}

/**
 * @brief List available system audio devices.
 *
 * @return Array of DeviceInfo objects: { name: string, description: string,
 * isOutput: boolean }
 */
Napi::Value ListAudioDevices(const Napi::CallbackInfo &info) {
  auto devices = DeviceManager::list_audio_devices();
  Napi::Array result = Napi::Array::New(info.Env(), devices.size());
  for (size_t i = 0; i < devices.size(); ++i) {
    Napi::Object obj = Napi::Object::New(info.Env());
    obj.Set("name", devices[i].name);
    obj.Set("description", devices[i].description);
    obj.Set("isOutput", devices[i].is_output);
    result[i] = obj;
  }
  return result;
}

/**
 * @brief Quick offline loading helper.
 *
 * This is a convenience function that opens a file, decodes all samples,
 * and returns both metadata and samples in one go.
 *
 * @param info Callback info containing:
 *   - path (string): File path, URL, or device name.
 *   - options (object, optional): { outputSampleRate, outputNumChannels }
 * @return Object: { metadata: Metadata, samples: Float32Array[] }
 */
Napi::Value Load(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "String expected for path")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string path = info[0].As<Napi::String>().Utf8Value();
  AudioStreamOptions opts;

  if (info.Length() > 1 && info[1].IsObject()) {
    Napi::Object obj = info[1].As<Napi::Object>();
    if (obj.Has("outputSampleRate"))
      opts.output_sample_rate =
          obj.Get("outputSampleRate").As<Napi::Number>().Int32Value();
    if (obj.Has("outputNumChannels"))
      opts.output_num_channels =
          obj.Get("outputNumChannels").As<Napi::Number>().Int32Value();
  }

  try {
    AudioDecoder decoder(opts);
    decoder.open(path);
    auto meta = decoder.get_metadata();
    auto samples = decoder.get_samples();

    Napi::Object result = Napi::Object::New(env);
    result.Set("metadata", MetadataToJs(env, meta));
    result.Set("samples", SamplesToJs(env, samples));
    return result;
  } catch (const std::exception &e) {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
}

/**
 * @brief Get waveform summary for visualization.
 *
 * Returns min/max values for each pixel column to support zooming.
 *
 * @param info Callback info containing:
 *   - path (string): File path
 *   - samplesPerPixel (number): How many samples to compress into one pixel
 * @return Object: { metadata: Metadata, min: Float32Array[], max: Float32Array[] }
 */
Napi::Value GetWaveform(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsNumber()) {
    Napi::TypeError::New(env, "String path and Number samplesPerPixel expected")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::string path = info[0].As<Napi::String>().Utf8Value();
  int32_t samplesPerPixel = info[1].As<Napi::Number>().Int32Value();
  if (samplesPerPixel < 1) samplesPerPixel = 1;

  AudioStreamOptions opts;
  // Limit output sample rate to reduce decoding overhead for waveform only
  opts.output_sample_rate = 16000; 

  try {
    AudioDecoder decoder(opts);
    decoder.open(path);
    auto meta = decoder.get_metadata();
    auto all_samples = decoder.get_samples();

    if (all_samples.empty()) return env.Undefined();

    size_t numChannels = all_samples.size();
    size_t totalSamples = all_samples[0].size();
    size_t numPixels = (totalSamples + samplesPerPixel - 1) / samplesPerPixel;

    Napi::Array minArr = Napi::Array::New(env, numChannels);
    Napi::Array maxArr = Napi::Array::New(env, numChannels);

    for (size_t c = 0; c < numChannels; ++c) {
      Napi::Float32Array minData = Napi::Float32Array::New(env, numPixels);
      Napi::Float32Array maxData = Napi::Float32Array::New(env, numPixels);
      
      const auto& channelData = all_samples[c];

      for (size_t p = 0; p < numPixels; ++p) {
        float minVal = 0.0f;
        float maxVal = 0.0f;
        size_t start = p * samplesPerPixel;
        size_t end = std::min(start + samplesPerPixel, totalSamples);

        if (start < totalSamples) {
          minVal = channelData[start];
          maxVal = channelData[start];
          for (size_t s = start + 1; s < end; ++s) {
            float val = channelData[s];
            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
          }
        }
        minData[p] = minVal;
        maxData[p] = maxVal;
      }
      minArr[c] = minData;
      maxArr[c] = maxData;
    }

    Napi::Object result = Napi::Object::New(env);
    result.Set("metadata", MetadataToJs(env, meta));
    result.Set("min", minArr);
    result.Set("max", maxArr);
    return result;
  } catch (const std::exception &e) {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
}

// --- AudioDecoder Class ---

/**
 * @class AudioDecoder
 * @brief State-managed audio decoder.
 *
 * Supports both full-file decoding and real-time streaming.
 */
class AudioDecoderAddon : public Napi::ObjectWrap<AudioDecoderAddon> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(
        env, "AudioDecoder",
        {InstanceMethod("load", &AudioDecoderAddon::Load),
         InstanceMethod("open", &AudioDecoderAddon::Open),
         InstanceMethod("push", &AudioDecoderAddon::Push),
         InstanceMethod("read", &AudioDecoderAddon::Read),
         InstanceMethod("getSamples", &AudioDecoderAddon::GetSamples),
         InstanceMethod("getMetadata", &AudioDecoderAddon::GetMetadata),
         InstanceMethod("isFinished", &AudioDecoderAddon::IsFinished)});
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("AudioDecoder", func);
    return exports;
  }

  /**
   * @brief Constructor for AudioDecoder
   * @param info Optional options object: {
   *   outputSampleRate?: number,
   *   outputNumChannels?: number,
   *   inputSampleRate?: number,
   *   inputChannels?: number,
   *   inputFormat?: string
   * }
   */
  AudioDecoderAddon(const Napi::CallbackInfo &info)
      : Napi::ObjectWrap<AudioDecoderAddon>(info) {
    AudioStreamOptions options;
    if (info.Length() > 0 && info[0].IsObject()) {
      Napi::Object obj = info[0].As<Napi::Object>();
      if (obj.Has("outputSampleRate"))
        options.output_sample_rate =
            obj.Get("outputSampleRate").As<Napi::Number>().Int32Value();
      if (obj.Has("outputNumChannels"))
        options.output_num_channels =
            obj.Get("outputNumChannels").As<Napi::Number>().Int32Value();
      if (obj.Has("inputSampleRate"))
        options.input_sample_rate =
            obj.Get("inputSampleRate").As<Napi::Number>().Int32Value();
      if (obj.Has("inputChannels"))
        options.input_channels =
            obj.Get("inputChannels").As<Napi::Number>().Int32Value();
      if (obj.Has("inputFormat"))
        options.input_format =
            obj.Get("inputFormat").As<Napi::String>().Utf8Value();
    }
    decoder = std::make_unique<AudioDecoder>(options);
  }

private:
  static Napi::FunctionReference constructor;
  std::unique_ptr<AudioDecoder> decoder;

  /**
   * @brief Open audio from file path, URL, device, or Buffer.
   * @param source String path or Buffer with full audio bytes
   * @return Metadata object
   */
  Napi::Value Load(const Napi::CallbackInfo &info) {
    if (info.Length() < 1) {
      Napi::TypeError::New(info.Env(), "String or Buffer expected")
          .ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
    try {
      if (info[0].IsString()) {
        decoder->open(info[0].As<Napi::String>().Utf8Value());
      } else if (info[0].IsBuffer()) {
        Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
        decoder->open(buf.Data(), buf.Length());
      } else {
        Napi::TypeError::New(info.Env(), "String or Buffer expected")
            .ThrowAsJavaScriptException();
        return info.Env().Undefined();
      }
      return MetadataToJs(info.Env(), decoder->get_metadata());
    } catch (const std::exception &e) {
      Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
  }

  /**
   * @brief Open audio (alias for load, doesn't return metadata).
   * @param source String path or Buffer with full audio bytes
   */
  void Open(const Napi::CallbackInfo &info) {
    if (info.Length() < 1) {
      Napi::TypeError::New(info.Env(), "String or Buffer expected")
          .ThrowAsJavaScriptException();
      return;
    }
    try {
      if (info[0].IsString()) {
        decoder->open(info[0].As<Napi::String>().Utf8Value());
      } else if (info[0].IsBuffer()) {
        Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
        decoder->open(buf.Data(), buf.Length());
      } else {
        Napi::TypeError::New(info.Env(), "String or Buffer expected")
            .ThrowAsJavaScriptException();
      }
    } catch (const std::exception &e) {
      Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    }
  }

  /**
   * @brief Get current metadata.
   * @return Metadata object
   */
  Napi::Value GetMetadata(const Napi::CallbackInfo &info) {
    try {
      return MetadataToJs(info.Env(), decoder->get_metadata());
    } catch (const std::exception &e) {
      Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
  }

  /**
   * @brief Push raw encoded bytes (streaming mode).
   * @param buffer Node.js Buffer containing encoded data
   */
  Napi::Value Push(const Napi::CallbackInfo &info) {
    if (info.Length() < 1 || !info[0].IsBuffer()) {
      Napi::TypeError::New(info.Env(), "Buffer expected")
          .ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
    Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
    try {
      decoder->push(buf.Data(), buf.Length());
    } catch (const std::exception &e) {
      Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    }
    return info.Env().Undefined();
  }

  /**
   * @brief Decode next available audio frame.
   * @return Array of Float32Array (channels), or null if EOF/no data
   */
  Napi::Value Read(const Napi::CallbackInfo &info) {
    try {
      auto frame = decoder->read();
      if (!frame)
        return info.Env().Null();

      Napi::Array channelsArr =
          Napi::Array::New(info.Env(), frame.num_channels);
      for (int c = 0; c < frame.num_channels; ++c) {
        Napi::Float32Array data =
            Napi::Float32Array::New(info.Env(), frame.num_samples);
        std::copy(frame.data[c], frame.data[c] + frame.num_samples,
                  data.Data());
        channelsArr[c] = data;
      }
      return channelsArr;
    } catch (const std::exception &e) {
      Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
      return info.Env().Null();
    }
  }

  /**
   * @brief Decode all remaining samples at once.
   * @return Array of Float32Array (one per channel)
   */
  Napi::Value GetSamples(const Napi::CallbackInfo &info) {
    try {
      auto samples = decoder->get_samples();
      return SamplesToJs(info.Env(), samples);
    } catch (const std::exception &e) {
      Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
      return Napi::Array::New(info.Env(), 0);
    }
  }

  /**
   * @brief Check if EOF has been reached.
   * @return boolean
   */
  Napi::Value IsFinished(const Napi::CallbackInfo &info) {
    return Napi::Boolean::New(info.Env(), decoder->is_finished());
  }
};

Napi::FunctionReference AudioDecoderAddon::constructor;

/**
 * @brief Initialize all exports for the native module.
 */
Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
  // Module-level functions
  exports.Set("setLogLevel", Napi::Function::New(env, SetLogLevel));
  exports.Set("listAudioDevices", Napi::Function::New(env, ListAudioDevices));
  exports.Set("load", Napi::Function::New(env, Load));
  exports.Set("getWaveform", Napi::Function::New(env, GetWaveform));

  // Classes
  AudioDecoderAddon::Init(env, exports);

  return exports;
}

NODE_API_MODULE(avioflow, InitAll)
