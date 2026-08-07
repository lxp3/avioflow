# AvioFlow

AvioFlow is a high-performance and easy-to-use streaming audio decoding library. 

The avioflow project is build on top of the FFMPEG library.

## Features

- **Audio format**: mp3, opus, flac, ogg, wav, m4a, aac. Anything FFmpeg supports — see the [Supported Formats Reference](doc/supported_formats.md) for the full decoder/encoder list.
- **Flexible Input**: Files, URLs, memory buffers, and real-time streams
- **Hardware Capture**: WASAPI loopback (system audio) and DirectShow (microphones)
- **Resampling**: Built-in sample rate conversion
- **Zero-copy API**: Direct buffer access via `FrameData` for maximum performance
- **Cross-platform**: Windows, Linux, macOS

## Speed

Decode time (best of 10 runs) for `public/wavs/TownTheme.mp3` (MP3, 44.1kHz
stereo, ~97.5s) on a single machine, decoding the full file into memory both
without resampling and with resampling to 16kHz. Numbers are for illustration
on this environment, not a formal cross-platform benchmark.

| Library        | No resample (ms) | Resample to 16kHz (ms) |
| -------------- | ----------------- | ----------------------- |
| **avioflow**       | **141.2**              | **132.6**                    |
| librosa        | 98.7               | 176.7                    |
| soundfile      | 100.6              | N/A (no built-in resampling) |
| torchcodec     | 130.8              | 148.9                    |
| sox (CLI)      | 206.0              | 403.7                    |
| ffmpeg (CLI)   | 167.7              | 198.9                    |

## Supported language

AvioFlow is packaged for several runtime and application environments. The
native core is shared across bindings, so behavior stays consistent whether you
embed it in a C++ service, call it from Python or JavaScript, ship it in a JVM
application, or run it in WebAssembly.

| Language / Runtime | Integration | Install / Consume | Compatibility |
| ------------------ | ----------- | ----------------- | ------------- |
| C++                | Native CMake package | `find_package(avioflow CONFIG REQUIRED)` | Shared/static packages for Linux, macOS, and Windows; Linux packages include both libstdc++ ABI 0 and ABI 1 variants |
| C / other FFI      | C ABI exported by the core | `#include <avioflow-c-api.h>` | Flat `extern "C"` surface with opaque handles; usable from any language with C FFI |
| Python             | pybind11 binding | `pip install avioflow` | Wheels for mainstream desktop/server platforms |
| JavaScript / Node.js | Node-API native addon | `npm install avioflow` | Platform-specific native packages selected by npm |
| Java               | JNI binding | Gradle / Maven | Runtime classifiers for Linux, macOS, and Windows |
| Rust               | C ABI binding | `cargo add avioflow` (see [rust/README.md](rust/README.md)) | Builds the native core from source; FFmpeg linked statically |
| WebAssembly        | WASM build | npm package / web bundle | Browser and WASM-capable runtime support |

## Decoder API Flow

AvioFlow uses the same pull-style output functions for offline and streaming
decoding. The difference is only how input bytes enter the decoder.

### Offline Input

```text
+-----------------------+
| AudioDecoder(options) |
+-----------+-----------+
            |
            v
+-----------------------------+
| load_file(path)             |
| load_buffer(bytes, size)    |
+-----------+-----------------+
            |
            v
+-----------------------------+
| get_frame()                          |  one decoded frame, zero-copy
| get_samples(start, stop)             |  samples in [start, stop), defaults to all
+-----------+-----------------+
            |
            v
+-----------------------------+
| is_finished()               |
+-----------------------------+
```

`get_samples(start_seconds, stop_seconds)` supports offline seek/time-range
decoding: pass a half-open range in seconds to decode only that window, or
call it with no arguments to decode the whole file. It may be called multiple
times on the same decoder to fetch different ranges; each call seeks
independently.

### Streaming Input

```text
+--------------------------------------+
| AudioDecoder(input_format, rate, ch) |
+-----------+--------------------------+
            |
            v
+-----------------------------+
| feed(chunk)                 |  first feed starts stream mode
+-----------+-----------------+
            |
            v
+-----------------------------+
| get_frame()                 |  returns empty if data is incomplete
| get_samples()               |  drains currently available samples (start/stop range not supported here)
+-----------+-----------------+
            | repeat feed/get_* while streaming
            v
+-----------------------------+
| flush()                     |  no more input; drain decoder delay
+-----------+-----------------+
            |
            v
+-----------------------------+
| get_samples() / get_frame() |  drain until is_finished()
+-----------------------------+
```

`flush()` does not discard data. It marks stream input complete so remaining
buffered bytes and codec-delayed frames can be drained.


## Build from Source

### Prerequisites
- CMake 3.20+
- Visual Studio 2022+ (Windows) or GCC 11+ (Linux)

### C++ Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build build --config Release
```

FFmpeg is fetched and configured automatically during the CMake configure step.

Run the tests:

```bash
ctest --test-dir build --output-on-failure
```

---

## Installation

Language packages have their own install instructions; see [Other language
APIs](#other-language-apis). The C++ package is below.

### C++ (CMake)

Download the C++ package for your platform and linkage, then point CMake at the
extracted package root with `CMAKE_PREFIX_PATH`.

```cmake
find_package(avioflow CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE avioflow::avioflow)
```

Release packages are split by linkage and platform:

Linux binaries target glibc 2.28 or newer. Select ABI 0 for the legacy
libstdc++ string ABI or ABI 1 for the C++11 string ABI; the eight Linux C++
archives use the filenames `avioflow-{shared|static}-linux-{x64|arm64}-abi{0|1}.tar.gz`.

- `avioflow-shared-linux-x64-abi1`, `avioflow-shared-linux-x64-abi0`
- `avioflow-static-linux-x64-abi1`, `avioflow-static-linux-x64-abi0`
- `avioflow-shared-linux-arm64-abi1`, `avioflow-shared-linux-arm64-abi0`
- `avioflow-static-linux-arm64-abi1`, `avioflow-static-linux-arm64-abi0`
- `avioflow-shared-macos-x64`, `avioflow-static-macos-x64`
- `avioflow-shared-macos-arm64`, `avioflow-static-macos-arm64`
- `avioflow-shared-win-x64`, `avioflow-static-win-x64`
- `avioflow-shared-win-arm64`, `avioflow-static-win-arm64`

Shared packages include the FFmpeg dynamic libraries needed at runtime. Static
packages include FFmpeg static libraries, transitive static dependency metadata,
and the bundled FFmpeg CMake package, so consumers do not need to configure
FFmpeg separately.

---

## C++ API

### Core Classes

#### `AudioDecoder`

Main class for audio decoding.

```cpp
#include "avioflow-cxx-api.h"
using namespace avioflow;

// Constructor options
AudioStreamOptions options;
options.output_sample_rate = 16000;    // Target sample rate
options.input_format = "s16le";        // For streaming: source format
options.input_sample_rate = 48000;     // For streaming: source rate
options.input_channels = 2;            // For streaming: source channels

AudioDecoder decoder(options);
```

#### Methods

| Method             | Description                                                      |
| ------------------ | ---------------------------------------------------------------- |
| `load_file(source)` | Load file path, URL, or device and return metadata              |
| `load_buffer(data, size)` | Load complete audio bytes from memory                    |
| `feed(data, size)` | Feed stream bytes; first feed starts stream mode                 |
| `flush()`          | Mark stream input complete and allow draining                    |
| `get_frame()`      | Decode next frame, returns `FrameData`                           |
| `get_samples(start_seconds=0.0, stop_seconds=nullopt)` | Decode samples in `[start_seconds, stop_seconds)` (offline mode); with no args, drains all currently available samples |
| `get_metadata()`   | Get audio metadata                                               |
| `is_finished()`    | Check if EOF reached                                             |

#### `FrameData`

Zero-copy frame data structure returned by `get_frame()`.

```cpp
struct FrameData {
    float** data;        // Planar channel pointers: data[channel][sample]
    int num_channels;    // Number of channels
    int num_samples;     // Samples per channel

    operator bool();     // True if valid data
};
```

> ⚠️ **Warning**: `FrameData.data` points to internal buffer, valid only until next `get_frame()` or `get_samples()` call.

### Examples

#### File Decoding (Offline)
```cpp
AudioDecoder decoder({.output_sample_rate = 16000});
decoder.load_file("audio.mp3");

auto samples = decoder.get_samples();  // vector<vector<float>>
std::cout << "Channels: " << samples.size() << std::endl;
std::cout << "Samples: " << samples[0].size() << std::endl;
```

#### Frame-by-Frame Decoding
```cpp
AudioDecoder decoder;
decoder.load_file("audio.mp3");

while (auto frame = decoder.get_frame()) {
    // frame.data[channel][sample]
    for (int c = 0; c < frame.num_channels; c++) {
        process(frame.data[c], frame.num_samples);
    }
}
```

#### Raw PCM Memory Decode
```cpp
// Raw PCM bytes have no container/header, so provide the input format details.
// Use FFmpeg demuxer format names such as "s16le", not codec names like
// "pcm_s16le".
AudioStreamOptions opts;
opts.input_format = "s16le";       // Signed 16-bit little-endian PCM
opts.input_sample_rate = 8000;     // 8 kHz
opts.input_channels = 1;           // Mono

AudioDecoder decoder(opts);
decoder.load_buffer(pcm_bytes, pcm_size); // Full PCM buffer in memory

while (auto frame = decoder.get_frame()) {
    // Output samples are float planar: frame.data[channel][sample]
    process(frame.data[0], frame.num_samples);
}
```

#### Time-Range Decoding (Offline Seek)
```cpp
AudioDecoder decoder;
decoder.load_file("audio.mp3");

// Decode only seconds 10.3 to 20.3
auto samples = decoder.get_samples(10.3, 20.3);

// Can be called again with a different range on the same decoder
auto next_range = decoder.get_samples(30.0, 40.0);
```

#### Streaming Decode (Push-based)
```cpp
AudioStreamOptions opts;
opts.input_format = "s16le";
opts.input_sample_rate = 48000;
opts.input_channels = 2;

AudioDecoder decoder(opts);
decoder.feed(raw_bytes, size);  // First feed starts stream mode

auto samples = decoder.get_samples(); // Decode all buffered data
// Or frame-by-frame:
while (auto frame = decoder.get_frame()) {
    // Process decoded audio...
}
decoder.flush();
```

---

## Other language APIs

Each binding has its own reference, kept next to the code it documents:

- [Python](python/README.md) — `pip install avioflow`
- [Node.js](nodejs/README.md) — `npm install avioflow`
- [Java](java/README.md) — Gradle / Maven
- [Rust](rust/README.md) — `cargo add avioflow`
- [WebAssembly](wasm/README.md) — browser and Electron builds
- [C ABI](avioflow/include/avioflow-c-api.h) — flat `extern "C"` surface for any language with C FFI

---

## License

MIT License
