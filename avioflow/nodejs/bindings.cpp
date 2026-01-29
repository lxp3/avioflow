#include "avioflow-cxx-api.h"
#include <napi.h>


// --- DeviceManager ---

Napi::Value ListAudioDevices(const Napi::CallbackInfo &info) {
  auto devices = avioflow::DeviceManager::list_audio_devices();
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

// --- AudioDecoder ---

class AudioDecoderAddon : public Napi::ObjectWrap<AudioDecoderAddon> {
public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(
        env, "AudioDecoder",
        {InstanceMethod("load", &AudioDecoderAddon::Load),
         InstanceMethod("push", &AudioDecoderAddon::Push),
         InstanceMethod("decodeNext", &AudioDecoderAddon::DecodeNext),
         InstanceMethod("isFinished", &AudioDecoderAddon::IsFinished)});
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("AudioDecoder", func);
    return exports;
  }

  AudioDecoderAddon(const Napi::CallbackInfo &info)
      : Napi::ObjectWrap<AudioDecoderAddon>(info) {
    avioflow::AudioStreamOptions options;
    if (info.Length() > 0 && info[0].IsObject()) {
      Napi::Object obj = info[0].As<Napi::Object>();
      if (obj.Has("outputSampleRate")) options.output_sample_rate = obj.Get("outputSampleRate").As<Napi::Number>().Int32Value();
      if (obj.Has("outputNumChannels")) options.output_num_channels = obj.Get("outputNumChannels").As<Napi::Number>().Int32Value();
      if (obj.Has("inputSampleRate")) options.input_sample_rate = obj.Get("inputSampleRate").As<Napi::Number>().Int32Value();
      if (obj.Has("inputChannels")) options.input_channels = obj.Get("inputChannels").As<Napi::Number>().Int32Value();
      if (obj.Has("inputFormat")) options.input_format = obj.Get("inputFormat").As<Napi::String>().Utf8Value();
    }
    decoder = std::make_unique<avioflow::AudioDecoder>(options);
  }

private:
  static Napi::FunctionReference constructor;
  std::unique_ptr<avioflow::AudioDecoder> decoder;

  Napi::Value Load(const Napi::CallbackInfo &info) {
    if (info.Length() < 1 || !info[0].IsString()) {
      Napi::TypeError::New(info.Env(), "String expected").ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
    decoder->open(info[0].As<Napi::String>().Utf8Value());

    auto meta = decoder->get_metadata();
    Napi::Object obj = Napi::Object::New(info.Env());
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

  Napi::Value Push(const Napi::CallbackInfo &info) {
    if (info.Length() < 1 || !info[0].IsBuffer()) {
      Napi::TypeError::New(info.Env(), "Buffer expected").ThrowAsJavaScriptException();
      return info.Env().Undefined();
    }
    Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
    decoder->push(buf.Data(), buf.Length());
    return info.Env().Undefined();
  }

  Napi::Value DecodeNext(const Napi::CallbackInfo &info) {
    auto frame = decoder->decode_next();
    if (!frame)
      return info.Env().Null();

    Napi::Array channelsArr = Napi::Array::New(info.Env(), frame.num_channels);
    for (int c = 0; c < frame.num_channels; ++c) {
      Napi::Float32Array data =
          Napi::Float32Array::New(info.Env(), frame.num_samples);
      std::copy(frame.data[c], frame.data[c] + frame.num_samples, data.Data());
      channelsArr[c] = data;
    }
    return channelsArr;
  }

  Napi::Value IsFinished(const Napi::CallbackInfo &info) {
    return Napi::Boolean::New(info.Env(), decoder->is_finished());
  }
};

Napi::FunctionReference AudioDecoderAddon::constructor;

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
  exports.Set("listAudioDevices", Napi::Function::New(env, ListAudioDevices));
  AudioDecoderAddon::Init(env, exports);
  return exports;
}

NODE_API_MODULE(avioflow, InitAll)
