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
 * @brief Read an array of Float32Array into planar sample vectors.
 *
 * Throws a JavaScript TypeError and returns false if the shape is wrong, so
 * callers must return immediately when this returns false.
 */
bool JsToSamples(Napi::Env env, Napi::Value value,
                 std::vector<std::vector<float>> &out) {
  if (!value.IsArray()) {
    Napi::TypeError::New(env, "samples must be an array of Float32Array")
        .ThrowAsJavaScriptException();
    return false;
  }

  Napi::Array channels = value.As<Napi::Array>();
  out.assign(channels.Length(), {});
  for (uint32_t channel = 0; channel < channels.Length(); ++channel) {
    Napi::Value entry = channels.Get(channel);
    if (!entry.IsTypedArray() ||
        entry.As<Napi::TypedArray>().TypedArrayType() != napi_float32_array) {
      Napi::TypeError::New(env, "Every channel must be a Float32Array")
          .ThrowAsJavaScriptException();
      return false;
    }
    Napi::Float32Array data = entry.As<Napi::Float32Array>();
    out[channel].assign(data.Data(), data.Data() + data.ElementLength());
  }
  return true;
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

Napi::Array StringsToJs(Napi::Env env, const std::vector<std::string> &values) {
  Napi::Array result = Napi::Array::New(env, values.size());
  for (size_t i = 0; i < values.size(); ++i)
    result.Set(i, values[i]);
  return result;
}

Napi::Value GetSupportedDecoders(const Napi::CallbackInfo &info) {
  return StringsToJs(info.Env(), get_supported_decoders());
}
Napi::Value GetSupportedEncoders(const Napi::CallbackInfo &info) {
  return StringsToJs(info.Env(), get_supported_encoders());
}
Napi::Value GetSupportedInputFormats(const Napi::CallbackInfo &info) {
  return StringsToJs(info.Env(), get_supported_input_formats());
}
Napi::Value GetSupportedOutputFormats(const Napi::CallbackInfo &info) {
  return StringsToJs(info.Env(), get_supported_output_formats());
}

Napi::Value Save(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsArray()) {
    Napi::TypeError::New(env, "String path and array of Float32Array expected")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::vector<std::vector<float>> samples;
  if (!JsToSamples(env, info[1], samples)) {
    return env.Undefined();
  }

  AudioWriteOptions options;
  if (info.Length() > 2 && info[2].IsObject()) {
    Napi::Object object = info[2].As<Napi::Object>();
    if (object.Has("codecName"))
      options.codec_name = object.Get("codecName").As<Napi::String>().Utf8Value();
    if (object.Has("containerFormat"))
      options.container_format = object.Get("containerFormat").As<Napi::String>().Utf8Value();
    if (object.Has("sampleRate"))
      options.sample_rate = object.Get("sampleRate").As<Napi::Number>().Int32Value();
    if (object.Has("numChannels"))
      options.num_channels = object.Get("numChannels").As<Napi::Number>().Int32Value();
    if (object.Has("bitRate"))
      options.bit_rate = object.Get("bitRate").As<Napi::Number>().Int64Value();
    if (object.Has("sampleFormat"))
      options.sample_format = object.Get("sampleFormat").As<Napi::String>().Utf8Value();
    if (object.Has("overwrite"))
      options.overwrite = object.Get("overwrite").As<Napi::Boolean>().Value();
  }

  try {
    save_audio(info[0].As<Napi::String>().Utf8Value(), samples, options);
  } catch (const std::exception &error) {
    Napi::Error::New(env, error.what()).ThrowAsJavaScriptException();
  }
  return env.Undefined();
}

/**
 * @brief Resample a complete buffer in one call.
 *
 * @param info Callback info containing:
 *   - samples (Float32Array[]): input, one array per channel
 *   - inputSampleRate (number)
 *   - outputSampleRate (number)
 *   - outputNumChannels (number, optional): defaults to the input count
 */
Napi::Value Resample(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() < 3 || !info[1].IsNumber() || !info[2].IsNumber()) {
    Napi::TypeError::New(env,
        "Expected (samples, inputSampleRate, outputSampleRate[, outputNumChannels])")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  std::vector<std::vector<float>> samples;
  if (!JsToSamples(env, info[0], samples)) {
    return env.Undefined();
  }

  std::optional<int> output_num_channels;
  if (info.Length() > 3 && info[3].IsNumber()) {
    output_num_channels = info[3].As<Napi::Number>().Int32Value();
  }

  try {
    auto output = resample(samples,
                           info[1].As<Napi::Number>().Int32Value(),
                           info[2].As<Napi::Number>().Int32Value(),
                           output_num_channels);
    return SamplesToJs(env, output);
  } catch (const std::exception &error) {
    Napi::Error::New(env, error.what()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
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
    decoder.load_file(path);
    auto samples = decoder.get_samples();
    auto meta = decoder.get_metadata();

    Napi::Object result = Napi::Object::New(env);
    result.Set("metadata", MetadataToJs(env, meta));
    result.Set("samples", SamplesToJs(env, samples));
    return result;
  } catch (const std::exception &e) {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
    return env.Undefined();
  }
}

class LoadAsyncWorker : public Napi::AsyncWorker {
public:
  LoadAsyncWorker(Napi::Env env, std::string path, AudioStreamOptions options)
      : Napi::AsyncWorker(env), deferred_(Napi::Promise::Deferred::New(env)),
        path_(std::move(path)), options_(std::move(options)) {}

  Napi::Promise Promise() const { return deferred_.Promise(); }

  void Execute() override {
    try {
      AudioDecoder decoder(options_);
      metadata_ = decoder.load_file(path_);
      samples_ = decoder.get_samples();
    } catch (const std::exception &error) {
      SetError(error.what());
    }
  }

  void OnOK() override {
    Napi::Object result = Napi::Object::New(Env());
    result.Set("metadata", MetadataToJs(Env(), metadata_));
    result.Set("samples", SamplesToJs(Env(), samples_));
    deferred_.Resolve(result);
  }

  void OnError(const Napi::Error &error) override { deferred_.Reject(error.Value()); }

private:
  Napi::Promise::Deferred deferred_;
  std::string path_;
  AudioStreamOptions options_;
  Metadata metadata_;
  std::vector<std::vector<float>> samples_;
};

Napi::Value LoadAsync(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "String expected for path")
        .ThrowAsJavaScriptException();
    return env.Undefined();
  }

  AudioStreamOptions options;
  if (info.Length() > 1 && info[1].IsObject()) {
    Napi::Object object = info[1].As<Napi::Object>();
    if (object.Has("outputSampleRate"))
      options.output_sample_rate = object.Get("outputSampleRate").As<Napi::Number>().Int32Value();
    if (object.Has("outputNumChannels"))
      options.output_num_channels = object.Get("outputNumChannels").As<Napi::Number>().Int32Value();
  }

  auto *worker = new LoadAsyncWorker(
      env, info[0].As<Napi::String>().Utf8Value(), options);
  Napi::Promise promise = worker->Promise();
  worker->Queue();
  return promise;
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
    decoder.load_file(path);
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
        {InstanceMethod("loadFile", &AudioDecoderAddon::LoadFile),
         InstanceMethod("loadBuffer", &AudioDecoderAddon::LoadBuffer),
         InstanceMethod("feed", &AudioDecoderAddon::Feed),
         InstanceMethod("flush", &AudioDecoderAddon::Flush),
         InstanceMethod("getFrame", &AudioDecoderAddon::GetFrame),
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
  Napi::Value LoadFile(const Napi::CallbackInfo &info) {
    if (info.Length() < 1 || !info[0].IsString()) {
      Napi::TypeError::New(info.Env(), "String expected")
          .ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
    try {
      decoder->load_file(info[0].As<Napi::String>().Utf8Value());
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
  Napi::Value LoadBuffer(const Napi::CallbackInfo &info) {
    if (info.Length() < 1 || !info[0].IsBuffer()) {
      Napi::TypeError::New(info.Env(), "Buffer expected")
          .ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
    try {
      Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
      decoder->load_buffer(buf.Data(), buf.Length());
      return MetadataToJs(info.Env(), decoder->get_metadata());
    } catch (const std::exception &e) {
      Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
      return info.Env().Undefined();
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
  Napi::Value Feed(const Napi::CallbackInfo &info) {
    if (info.Length() < 1 || !info[0].IsBuffer()) {
      Napi::TypeError::New(info.Env(), "Buffer expected")
          .ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
    Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
    try {
      decoder->feed(buf.Data(), buf.Length());
    } catch (const std::exception &e) {
      Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    }
    return info.Env().Undefined();
  }

  Napi::Value Flush(const Napi::CallbackInfo &info) {
    try {
      decoder->flush();
    } catch (const std::exception &e) {
      Napi::Error::New(info.Env(), e.what()).ThrowAsJavaScriptException();
    }
    return info.Env().Undefined();
  }

  /**
   * @brief Decode next available audio frame.
   * @return Array of Float32Array (channels), or null if EOF/no data
   */
  Napi::Value GetFrame(const Napi::CallbackInfo &info) {
    try {
      auto frame = decoder->get_frame();
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
      double start_seconds = 0.0;
      std::optional<double> stop_seconds = std::nullopt;
      if (info.Length() > 0 && info[0].IsNumber()) {
        start_seconds = info[0].As<Napi::Number>().DoubleValue();
      }
      if (info.Length() > 1 && info[1].IsNumber()) {
        stop_seconds = info[1].As<Napi::Number>().DoubleValue();
      }
      auto samples = decoder->get_samples(start_seconds, stop_seconds);
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

// --- AudioResampler Class ---

/**
 * @class AudioResampler
 * @brief Stateful resampler for audio arriving in chunks.
 *
 * Filter state is kept across process() calls, so consecutive chunks join
 * without discontinuities. flush() must be called at the end, or the last few
 * milliseconds of audio are lost.
 */
class AudioResamplerAddon : public Napi::ObjectWrap<AudioResamplerAddon> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(
        env, "AudioResampler",
        {InstanceMethod("process", &AudioResamplerAddon::Process),
         InstanceMethod("flush", &AudioResamplerAddon::Flush),
         InstanceMethod("outputSampleRate", &AudioResamplerAddon::OutputSampleRate),
         InstanceMethod("outputNumChannels", &AudioResamplerAddon::OutputNumChannels)});
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("AudioResampler", func);
    return exports;
  }

  /**
   * @brief Constructor.
   * @param info Options object: {
   *   inputSampleRate: number,
   *   outputSampleRate: number,
   *   outputNumChannels?: number
   * }
   */
  AudioResamplerAddon(const Napi::CallbackInfo &info)
      : Napi::ObjectWrap<AudioResamplerAddon>(info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsObject()) {
      Napi::TypeError::New(
          env, "Expected an options object with inputSampleRate and outputSampleRate")
          .ThrowAsJavaScriptException();
      return;
    }

    Napi::Object obj = info[0].As<Napi::Object>();
    AudioResampleOptions options;
    if (obj.Has("inputSampleRate"))
      options.input_sample_rate =
          obj.Get("inputSampleRate").As<Napi::Number>().Int32Value();
    if (obj.Has("outputSampleRate"))
      options.output_sample_rate =
          obj.Get("outputSampleRate").As<Napi::Number>().Int32Value();
    if (obj.Has("outputNumChannels"))
      options.output_num_channels =
          obj.Get("outputNumChannels").As<Napi::Number>().Int32Value();

    // The C++ constructor validates the rates, so surface its message rather
    // than duplicating the checks here.
    try {
      resampler = std::make_unique<AudioResampler>(options);
    } catch (const std::exception &error) {
      Napi::Error::New(env, error.what()).ThrowAsJavaScriptException();
    }
  }

private:
  static Napi::FunctionReference constructor;
  std::unique_ptr<AudioResampler> resampler;

  /**
   * @brief Resample one chunk.
   * @param info Callback info containing samples (Float32Array[])
   * @return Array of Float32Array, possibly shorter than the ratio suggests
   */
  Napi::Value Process(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1) {
      Napi::TypeError::New(env, "Expected samples as an array of Float32Array")
          .ThrowAsJavaScriptException();
      return env.Undefined();
    }

    std::vector<std::vector<float>> samples;
    if (!JsToSamples(env, info[0], samples)) {
      return env.Undefined();
    }

    try {
      return SamplesToJs(env, resampler->process(samples));
    } catch (const std::exception &error) {
      Napi::Error::New(env, error.what()).ThrowAsJavaScriptException();
      return env.Undefined();
    }
  }

  /**
   * @brief Drain buffered samples. Call once after the final process().
   * @return Array of Float32Array with the remaining samples
   */
  Napi::Value Flush(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    try {
      return SamplesToJs(env, resampler->flush());
    } catch (const std::exception &error) {
      Napi::Error::New(env, error.what()).ThrowAsJavaScriptException();
      return env.Undefined();
    }
  }

  Napi::Value OutputSampleRate(const Napi::CallbackInfo &info) {
    return Napi::Number::New(info.Env(), resampler->output_sample_rate());
  }

  Napi::Value OutputNumChannels(const Napi::CallbackInfo &info) {
    return Napi::Number::New(info.Env(), resampler->output_num_channels());
  }
};

Napi::FunctionReference AudioResamplerAddon::constructor;

/**
 * @brief Initialize all exports for the native module.
 */
Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
  // Module-level functions
  exports.Set("setLogLevel", Napi::Function::New(env, SetLogLevel));
  exports.Set("listAudioDevices", Napi::Function::New(env, ListAudioDevices));
  exports.Set("load", Napi::Function::New(env, Load));
  exports.Set("loadAsync", Napi::Function::New(env, LoadAsync));
  exports.Set("getSupportedDecoders", Napi::Function::New(env, GetSupportedDecoders));
  exports.Set("getSupportedEncoders", Napi::Function::New(env, GetSupportedEncoders));
  exports.Set("getSupportedInputFormats", Napi::Function::New(env, GetSupportedInputFormats));
  exports.Set("getSupportedOutputFormats", Napi::Function::New(env, GetSupportedOutputFormats));
  exports.Set("save", Napi::Function::New(env, Save));
  exports.Set("getWaveform", Napi::Function::New(env, GetWaveform));
  exports.Set("resample", Napi::Function::New(env, Resample));

  // Classes
  AudioDecoderAddon::Init(env, exports);
  AudioResamplerAddon::Init(env, exports);

  return exports;
}

NODE_API_MODULE(avioflow, InitAll)
