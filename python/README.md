# avioflow (Python)

Python bindings for [AvioFlow](https://github.com/lxp3/avioflow), a
high-performance audio library built on FFmpeg.

The package wraps the same C++ core as the Node.js, Java, Rust and WebAssembly
bindings, through a pybind11 extension. FFmpeg ships inside the wheel, so there
is nothing to install or locate at runtime.

## Install

```bash
pip install avioflow
```

Wheels are published for CPython 3.8 through 3.14 on Linux (x86_64, aarch64),
macOS (x86_64, arm64) and Windows (x64, arm64).

## Conventions

- **Samples are NumPy arrays** shaped `(channels, samples)`, dtype `float32`.
  Channel 0 is the first channel: `samples[0]` is a 1-D array of one channel.
- **Failures raise exceptions.** A source that cannot be opened or decoded
  raises `RuntimeError`; a rejected argument raises `ValueError`.
- **Unset options preserve the source format.** Passing `None` or `-1` for a
  rate or channel count keeps whatever the input has.

## Quick start

```python
import avioflow

# Metadata only, no decoding
meta = avioflow.info("audio.mp3")
print(f"{meta.duration:.1f}s, {meta.sample_rate} Hz, {meta.num_channels} ch")

# Metadata plus all samples, in one call
meta, samples = avioflow.load("audio.mp3", output_sample_rate=16000)
print(samples.shape)  # (channels, samples)
```

## Module functions

| Function | Returns | Description |
| -------- | ------- | ----------- |
| `info(source)` | `Metadata` | Read metadata without decoding. |
| `load(source, output_sample_rate=None, output_num_channels=None)` | `(Metadata, ndarray)` | Open and decode everything in one call. |
| `save(path, samples, options=None)` | `None` | Write samples to a file. |
| `resample(samples, input_sample_rate, output_sample_rate, output_num_channels=None)` | `ndarray` | Resample a complete buffer. |
| `set_log_level(level)` | `None` | FFmpeg verbosity. |
| `get_supported_decoders()` | `list[str]` | Available decoders, e.g. `"mp3"`. |
| `get_supported_encoders()` | `list[str]` | Available encoders, e.g. `"pcm_s16le"`. |
| `get_supported_input_formats()` | `list[str]` | Available demuxers. |
| `get_supported_output_formats()` | `list[str]` | Available muxers. |
| `DeviceManager.list_audio_devices()` | `list[DeviceInfo]` | Enumerate audio devices. |

`source` accepts a path, a `pathlib.Path`, a URL, a device identifier, or
encoded audio as `bytes`, `bytearray`, `memoryview` or `io.BytesIO`.

## Decoding

### `AudioDecoder`

```python
decoder = avioflow.AudioDecoder(
    output_sample_rate=16000,   # resample; omit to keep the source rate
    output_num_channels=1,      # remix; omit to keep the source channels
    input_format="s16le",       # required for streaming
    input_sample_rate=48000,    # required for raw PCM streaming
    input_channels=2,           # required for raw PCM streaming
)
```

| Method | Returns | Description |
| ------ | ------- | ----------- |
| `load_file(source)` | `Metadata` | Open a path, URL or device. |
| `load_buffer(source)` | `Metadata` | Open complete file bytes in memory. |
| `feed(data)` | `None` | Push encoded bytes for streaming decode. |
| `flush()` | `None` | Mark streaming input complete. |
| `get_samples(start_seconds=0.0, stop_seconds=None)` | `ndarray` | Decode `[start, stop)` in seconds. |
| `get_frame()` | `ndarray \| None` | Decode one frame; `None` at end of stream. |
| `is_finished()` | `bool` | Whether the stream is exhausted. |
| `get_metadata()` | `Metadata` | Current stream metadata. |

### Offline decoding

```python
decoder = avioflow.AudioDecoder(output_sample_rate=16000)
meta = decoder.load_file("audio.mp3")
samples = decoder.get_samples()

print(f"{samples.shape[0]} channels x {samples.shape[1]} samples")
```

`load_file` reports the *source* stream. The resampler is not configured until
the first frame is decoded, so when `output_sample_rate` is set the new rate
appears in `get_metadata()` after decoding rather than in the value `load_file`
returns.

### Time-range decoding

```python
decoder = avioflow.AudioDecoder()
decoder.load_file("audio.mp3")

# Seconds 10.3 through 20.3, exclusive of the end
window = decoder.get_samples(10.3, 20.3)

# From 30s to the end
tail = decoder.get_samples(30.0)
```

Each call seeks independently, so one decoder can serve many ranges. Range
decoding requires offline mode; in stream mode `get_samples()` drains whatever
is currently buffered and the range arguments do not apply.

### Frame-by-frame

```python
decoder = avioflow.AudioDecoder()
decoder.load_file("audio.mp3")

total = 0
while (frame := decoder.get_frame()) is not None:
    total += frame.shape[1]       # frame is (channels, samples)
```

### Streaming decode

```python
decoder = avioflow.AudioDecoder(input_format="mp3")

while chunk := stream.read(4096):
    decoder.feed(chunk)
    samples = decoder.get_samples()
    if samples.size:
        process(samples)

decoder.flush()                   # then drain the remainder
remaining = decoder.get_samples()
```

`flush()` discards nothing. It marks input complete so buffered bytes and
codec-delayed frames can still be drained. For raw PCM input, also pass
`input_sample_rate` and `input_channels`.

## Encoding

```python
avioflow.save("out.wav", samples, avioflow.AudioWriteOptions(
    container_format="wav",
    codec_name="pcm_s16le",
    sample_rate=16000,
))
```

`samples` is any array-like shaped `(channels, samples)`; it is converted to
`float32` as needed.

### `AudioWriteOptions`

| Attribute | Common values |
| --------- | ------------- |
| `codec_name` | `"pcm_s16le"`, `"flac"`, `"aac"`, `"libmp3lame"`, `"libopus"` |
| `container_format` | `"wav"`, `"flac"`, `"mp4"`, `"ogg"`, `"adts"` |
| `sample_format` | `"s16"`, `"s32"`, `"flt"`, `"fltp"` |
| `sample_rate` | 8000, 16000, 44100, 48000 |
| `num_channels` | 1, 2 |
| `bit_rate` | 128000, 192000, 320000 |
| `overwrite` | Defaults to `True` |

Unset fields are inferred from the container and the input samples. A format
preset is also available:

```python
options = avioflow.AudioWriteOptions("wav", 16000, 1)   # format, rate, channels
```

## Resampling

Two entry points: `resample` for a buffer held in full, `AudioResampler` for
audio arriving in chunks. Both work on any array, not only avioflow output, so a
NumPy array from another library can be resampled directly.

To resample *while decoding*, pass `output_sample_rate` to `AudioDecoder`
instead; that avoids a second pass over the samples.

### One-shot

```python
downsampled = avioflow.resample(samples, 44100, 16000)
mono = avioflow.resample(samples, 44100, 16000, output_num_channels=1)
```

### Chunked

```python
import numpy as np

resampler = avioflow.AudioResampler(44100, 16000)
parts = [resampler.process(chunk) for chunk in chunks]
parts.append(resampler.flush())
out = np.concatenate([p for p in parts if p.size], axis=1)
```

`flush()` is not optional. The resampler holds back the last few milliseconds to
keep filter continuity, and skipping the flush discards them. Filter state
carries across `process` calls, so chunked output matches a one-shot conversion
sample for sample — calling `resample` per chunk instead would introduce a
discontinuity at every boundary.

`output_num_channels` returns 0 until the first `process` call reveals the input
channel count. The channel count must not change between calls.

## Metadata

```python
meta = avioflow.info("audio.mp3")
```

| Attribute | Type | Description |
| --------- | ---- | ----------- |
| `duration` | `float` | Seconds; 0 for live streams. |
| `num_samples` | `int` | Samples per channel; updated at EOF for streams. |
| `sample_rate` | `int` | Hz. |
| `num_channels` | `int` | 1 for mono, 2 for stereo. |
| `sample_format` | `str` | Source format, e.g. `"fltp"`. |
| `codec` | `str` | Decoder name, e.g. `"mp3float"`. |
| `bit_rate` | `int` | Bits per second. |
| `container` | `str` | Container format, e.g. `"mp3"`. |

Note `codec` is the FFmpeg *decoder* name, so an MP3 file reports
`"mp3float"` while `container` reports `"mp3"`.

## Devices and diagnostics

```python
for dev in avioflow.DeviceManager.list_audio_devices():
    print(f"{dev.name}: {dev.description} (output={dev.is_output})")

# Open a device by passing its name as the source
decoder = avioflow.AudioDecoder()
decoder.load_file("audio=Microphone")

avioflow.set_log_level("debug")   # quiet, error, warning, info, debug, trace
```

## Build from source

```bash
pip install ./python
```

Building compiles the C++ core, so the host needs a C++17 compiler and CMake
3.20 or newer. FFmpeg is fetched automatically during the CMake configure step.

Run the tests against a checkout:

```bash
python python/tests/test_offline_load.py public/wavs/TownTheme.mp3
```

## License

MIT
