#include "avioflow-cxx-api.h"
#include "../core/ffmpeg/single-stream-decoder.h"

namespace avioflow
{

  // PIMPL Implementation
  class AudioDecoder::Impl
  {
  public:
    explicit Impl(const AudioStreamOptions &options)
        : decoder_(options) {}

    SingleStreamDecoder decoder_;
    Metadata cached_metadata_;
  };

  // Constructor
  AudioDecoder::AudioDecoder(const AudioStreamOptions &options)
      : impl_(std::make_unique<Impl>(options)) {}

  // Destructor
  AudioDecoder::~AudioDecoder() = default;

  // Move constructor
  AudioDecoder::AudioDecoder(AudioDecoder &&) noexcept = default;

  // Move assignment
  AudioDecoder &AudioDecoder::operator=(AudioDecoder &&) noexcept = default;

  // --- Input Methods ---

  void AudioDecoder::open(const std::string &source)
  {
    impl_->decoder_.open(source);
    impl_->cached_metadata_ = impl_->decoder_.get_metadata();
  }

  void AudioDecoder::open_memory(const uint8_t *data, size_t size)
  {
    impl_->decoder_.open_memory(data, size);
    impl_->cached_metadata_ = impl_->decoder_.get_metadata();
  }

  void AudioDecoder::open_stream(AVIOReadCallback avio_read_callback)
  {
    impl_->decoder_.open_stream(std::move(avio_read_callback));
    impl_->cached_metadata_ = impl_->decoder_.get_metadata();
  }

  // --- Decoding Methods ---

  AudioSamples AudioDecoder::decode_next()
  {
    AudioSamples result;

    AVFrame *frame = impl_->decoder_.decode_next();
    if (!frame)
      return result; // Empty samples

    result.sample_rate = frame->sample_rate;
    int num_channels = frame->ch_layout.nb_channels;
    result.data.resize(num_channels);

    for (int c = 0; c < num_channels; ++c)
    {
      const float *channel_data = reinterpret_cast<const float *>(frame->data[c]);
      result.data[c].assign(channel_data, channel_data + frame->nb_samples);
    }

    return result;
  }

  AudioSamples AudioDecoder::get_all_samples()
  {
    return impl_->decoder_.get_all_samples();
  }

  // --- Status ---

  bool AudioDecoder::is_finished() const
  {
    return impl_->decoder_.is_finished();
  }

  const Metadata &AudioDecoder::get_metadata() const
  {
    return impl_->decoder_.get_metadata();
  }

} // namespace avioflow
