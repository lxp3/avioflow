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
        {InstanceMethod("open", &AudioDecoderAddon::Open),
         InstanceMethod("decodeNext", &AudioDecoderAddon::DecodeNext),
         InstanceMethod("getMetadata", &AudioDecoderAddon::GetMetadata),
         InstanceMethod("isFinished", &AudioDecoderAddon::IsFinished)});
    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("AudioDecoder", func);
    return exports;
  }

  AudioDecoderAddon(const Napi::CallbackInfo &info)
      : Napi::ObjectWrap<AudioDecoderAddon>(info),
        decoder(std::make_unique<avioflow::AudioDecoder>()) {}

private:
  static Napi::FunctionReference constructor;
  std::unique_ptr<avioflow::AudioDecoder> decoder;

  void Open(const Napi::CallbackInfo &info) {
    if (info.Length() < 1 || !info[0].IsString()) {
      Napi::TypeError::New(info.Env(), "String expected")
          .ThrowAsJavaScriptException();
      return;
    }
    decoder->open(info[0].As<Napi::String>().Utf8Value());
  }

  Napi::Value DecodeNext(const Napi::CallbackInfo &info) {
    auto samples = decoder->decode_next();
    if (samples.data.empty())
      return info.Env().Null();

    Napi::Object obj = Napi::Object::New(info.Env());
    obj.Set("sampleRate", samples.sample_rate);
    obj.Set("channels", static_cast<uint32_t>(samples.data.size()));

    Napi::Array channelsArr = Napi::Array::New(info.Env(), samples.data.size());
    for (size_t c = 0; c < samples.data.size(); ++c) {
      Napi::Float32Array data =
          Napi::Float32Array::New(info.Env(), samples.data[c].size());
      std::copy(samples.data[c].begin(), samples.data[c].end(), data.Data());
      channelsArr[c] = data;
    }
    obj.Set("data", channelsArr);
    return obj;
  }

  Napi::Value GetMetadata(const Napi::CallbackInfo &info) {
    auto meta = decoder->get_metadata();
    Napi::Object obj = Napi::Object::New(info.Env());
    obj.Set("duration", meta.duration);
    obj.Set("sampleRate", meta.sample_rate);
    obj.Set("numChannels", meta.num_channels);
    obj.Set("codec", meta.codec);
    return obj;
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
