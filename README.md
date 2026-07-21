# AvioFlow

AvioFlow is a high-performance and easy-to-use streaming audio decoding library.

## Features

- **Audio format**: mp3, opus, flac, ogg, wav, m4a, acc. Anything.
- **Flexible Input**: Files, URLs, memory buffers, and real-time streams
- **Hardware Capture**: WASAPI loopback (system audio) and DirectShow (microphones)
- **Resampling**: Built-in sample rate conversion
- **Zero-copy API**: Direct buffer access via `FrameData` for maximum performance
- **Cross-platform**: Windows, Linux, macOS

## Supported language

AvioFlow is packaged for several runtime and application environments. The
native core is shared across bindings, so behavior stays consistent whether you
embed it in a C++ service, call it from Python or JavaScript, ship it in a JVM
application, or run it in WebAssembly.

| Language / Runtime | Integration | Install / Consume | Compatibility |
| ------------------ | ----------- | ----------------- | ------------- |
| C++                | Native CMake package | `find_package(avioflow CONFIG REQUIRED)` | Shared/static packages for Linux, macOS, and Windows; Linux packages include both libstdc++ ABI 0 and ABI 1 variants |
| Python             | pybind11 binding | `pip install avioflow` | Wheels for mainstream desktop/server platforms |
| JavaScript / Node.js | Node-API native addon | `npm install avioflow` | Platform-specific native packages selected by npm |
| Java               | JNI binding | Gradle / Maven | Runtime classifiers for Linux, macOS, and Windows |
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
| get_frame()                 |  one decoded frame, zero-copy
| get_samples()               |  all currently available samples
+-----------+-----------------+
            |
            v
+-----------------------------+
| is_finished()               |
+-----------------------------+
```

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
| get_samples()               |  drains currently available samples
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

## Installation

### Python
```bash
pip install avioflow
```

### Java

Gradle users need the main Java API jar plus one native classifier for the target platform:

```kotlin
dependencies {
    implementation("io.github.lxp3:avioflow:0.5.2")
    runtimeOnly("io.github.lxp3:avioflow:0.5.2:linux-x86_64")
}
```

Maven:

```xml
<dependency>
  <groupId>io.github.lxp3</groupId>
  <artifactId>avioflow</artifactId>
  <version>0.3.2</version>
</dependency>
<dependency>
  <groupId>io.github.lxp3</groupId>
  <artifactId>avioflow</artifactId>
  <version>0.3.2</version>
  <classifier>linux-x86_64</classifier>
  <scope>runtime</scope>
</dependency>
```

Native classifiers: `linux-x86_64`, `linux-aarch64`, `macos-x86_64`, `macos-aarch64`, `windows-x86_64`, `windows-aarch64`.


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
| `get_samples()`    | Drain currently available samples                                |
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

## Python API

### `AudioDecoder`

```python
import avioflow

# Constructor with keyword arguments
decoder = avioflow.AudioDecoder(
    output_sample_rate=16000,    # Optional: target sample rate
    input_format="s16le",        # For streaming: source format
    input_sample_rate=48000,     # For streaming: source rate
    input_channels=2             # For streaming: source channels
)
```

#### Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `load_file(source)` | `Metadata` | Load file, URL, or `pathlib.Path` |
| `load_buffer(data)` | `Metadata` | Load complete bytes-like input |
| `feed(data)` | `None` | Feed streaming bytes |
| `flush()` | `None` | Mark stream input complete |
| `get_frame()` | `ndarray \| None` | Decode next frame |
| `get_samples()` | `ndarray` | Drain currently available samples |
| `is_finished()` | `bool` | Check if EOF |

#### `Metadata`

```python
# Quick metadata inspection without full decoding
meta = avioflow.info("audio.mp3")
print(f"Duration: {meta.duration}s")
print(f"Sample Rate: {meta.sample_rate}Hz")
print(f"Codec: {meta.codec}")

# Encoded audio bytes also work
with open("audio.mp3", "rb") as f:
    meta = avioflow.info(f.read())
```

### Examples

#### File Decoding
```python
decoder = avioflow.AudioDecoder(output_sample_rate=16000)
meta = decoder.load_file("speech.wav")
samples = decoder.get_samples()      # numpy array (channels, samples)
print(f"Shape: {samples.shape}")     # e.g., (1, 160000)
```

#### Streaming Decode
```python
decoder = avioflow.AudioDecoder(
    input_format="s16le",
    input_sample_rate=48000,
    input_channels=2
)

while True:
    data = socket.recv(4096)
    if not data:
        decoder.flush()
        break
    decoder.feed(data)
    samples = decoder.get_samples()
    if samples.size > 0:
        process_audio(samples)
```

#### Device Discovery
```python
devices = avioflow.DeviceManager.list_audio_devices()
for dev in devices:
    print(f"{dev.name}: {dev.description}")
```

### Logging
```python
avioflow.set_log_level("debug")  # quiet, error, warning, info, debug, trace
```

---

## Node.js API

### Compatibility

| Runtime           | Version         | Support                        |
| ----------------- | --------------- | ------------------------------ |
| **Node.js**       | 16, 18, 20, 22+ | ✅ Native (N-API)               |
| **Electron**      | All versions    | ✅ Supported (requires rebuild) |
| **Architectures** | x64             | ✅ Linux, Windows               |

### Installation

```bash
npm install avioflow
```

### ESM Import
```javascript
import avioflow from 'avioflow';
```

### Module-level Functions

| Function               | Returns               | Description                                                                |
| ---------------------- | --------------------- | -------------------------------------------------------------------------- |
| `load(path, options?)` | `{metadata, samples}` | **Convenience**: Opens, decodes all samples, and returns both in one call. |
| `listAudioDevices()`   | `DeviceInfo[]`        | List available system audio devices.                                       |
| `setLogLevel(level)`   | `void`                | Set FFmpeg log level ("quiet", "info", "debug", etc.).                     |

### `AudioDecoder`

```javascript
// Constructor with options object
const decoder = new avioflow.AudioDecoder({
    outputSampleRate: 16000,    // Optional: target sample rate
    outputNumChannels: 1,       // Optional: target channels
    inputFormat: 's16le',       // For streaming: source format
    inputSampleRate: 48000,     // For streaming: source rate
    inputChannels: 2            // For streaming: source channels
});
```

#### Methods

| Method         | Returns                    | Description                                       |
| -------------- | -------------------------- | ------------------------------------------------- |
| `loadFile(source)` | `Metadata`            | Load file, URL, or device name. Returns metadata. |
| `loadBuffer(buffer)` | `Metadata`          | Load complete encoded bytes from memory.          |
| `feed(buffer)` | `void`                     | Feed streaming bytes.                             |
| `flush()`      | `void`                     | Mark stream input complete.                       |
| `getFrame()`   | `Float32Array[]` \| `null` | Decode next frame. Returns array of channel data. |
| `getSamples()` | `Float32Array[]`           | Drain currently available samples.                |
| `isFinished()` | `boolean`                  | Check if end of stream reached.                   |

### Examples

#### Quick File Loading (Recommended)
```javascript
// Opens file, resamples to 16kHz mono, and decodes everything
const { metadata, samples } = avioflow.load("audio.mp3", {
    outputSampleRate: 16000,
    outputNumChannels: 1
});

console.log(`Duration: ${metadata.duration}s`);
console.log(`Channels: ${samples.length}, Samples: ${samples[0].length}`);
```

#### Batch Decoding with Decoder Instance
```javascript
const decoder = new avioflow.AudioDecoder({ outputSampleRate: 44100 });
const meta = decoder.loadFile("audio.wav");

// Decodes the entire file into memory
const allSamples = decoder.getSamples();
process(allSamples);
```

#### Streaming Decode (Real-time)
```javascript
const decoder = new avioflow.AudioDecoder({
    inputFormat: 's16le',
    inputSampleRate: 48000,
    inputChannels: 2
});

socket.on('data', (chunk) => {
    decoder.feed(chunk);

    // Get all samples decoded from this chunk
    const samples = decoder.getSamples();
    if (samples.length > 0) {
        processAudio(samples);
    }
});

socket.on('end', () => {
    decoder.flush();
    const remaining = decoder.getSamples();
    if (remaining.length > 0) {
        processAudio(remaining);
    }
});
```

#### Device Discovery
```javascript
const devices = avioflow.listAudioDevices();
devices.forEach(dev => {
    console.log(`${dev.isOutput ? 'Output' : 'Input'}: ${dev.name} (${dev.description})`);
});
```

---

## Java API

### File Decoding

```java
import io.github.lxp3.avioflow.AudioDecoder;
import io.github.lxp3.avioflow.AudioStreamOptions;

try (AudioDecoder decoder = new AudioDecoder(
        new AudioStreamOptions().outputSampleRate(16000))) {
    decoder.loadFile("audio.mp3");
    float[][] samples = decoder.getSamples();
    System.out.println(samples.length + " channels");
}
```

### Encoding

```java
import io.github.lxp3.avioflow.AudioEncoder;
import io.github.lxp3.avioflow.AudioWriteOptions;

AudioEncoder.saveAudio(
    "out.wav",
    samples,
    new AudioWriteOptions()
        .containerFormat("wav")
        .codecName("pcm_s16le")
        .sampleRate(16000)
);
```


---

## Build from Source

### Prerequisites
- CMake 3.20+
- Visual Studio 2022+ (Windows) or GCC 11+ (Linux)
- Python 3.8+ with pybind11 (for Python bindings)
- Node.js 16+ (for Node.js bindings)

### C++ & Python Build
```bash
./build.sh
```
This will configure and build the C++ library and Python bindings.

### Node.js Build
```bash
./build-nodejs.sh
```
This will build the Node.js bindings using `cmake-js` and run compatibility tests.

### Java Build
```bash
./build-java.sh linux-x86_64
```
This builds the JNI library and creates a platform classifier jar.

---

## Supported Formats

AvioFlow supports a wide range of audio formats, codecs, and devices through FFmpeg.

For a complete and detailed list of supported decoders, encoders, and input formats, please refer to the **[Supported Formats Reference](doc/supported_formats.md)**.

---

## License

MIT License
