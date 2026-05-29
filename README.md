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

| C++        |              |                      |     |     |
| ---------- | ------------ | -------------------- | --- | --- |
| python     | pybind11     | pip install avioflow |     |     |
| JavaScript | node-add-api | npm install avioflow |     |     |
| Java       | JNI          | Gradle/Maven         |     |     |
|            | wasm         |                      |     |     |
|            | vsix         |                      |     |     |


## Installation

### Python
```bash
pip install avioflow
```

### Java

Gradle users need the main Java API jar plus one native classifier for the target platform:

```kotlin
dependencies {
    implementation("io.github.lxp3:avioflow:0.3.4")
    runtimeOnly("io.github.lxp3:avioflow:0.3.4:linux-x86_64")
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

- `avioflow-shared-linux-x64`, `avioflow-static-linux-x64`
- `avioflow-shared-linux-arm64`, `avioflow-static-linux-arm64`
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
| `open(source)`     | Open file path, URL, or device                                   |
| `push(data, size)` | Push raw bytes for streaming decode                              |
| `read()`           | Decode next frame, returns `FrameData`. (Formerly `decode_next`) |
| `get_samples()`    | Decode all currently available samples. (Formerly `get_samples`) |
| `get_metadata()`   | Get audio metadata                                               |
| `is_finished()`    | Check if EOF reached                                             |

#### `FrameData`

Zero-copy frame data structure returned by `read()`.

```cpp
struct FrameData {
    float** data;        // Planar channel pointers: data[channel][sample]
    int num_channels;    // Number of channels
    int num_samples;     // Samples per channel

    operator bool();     // True if valid data
};
```

> ⚠️ **Warning**: `FrameData.data` points to internal buffer, valid only until next `read()` call.

### Examples

#### File Decoding (Offline)
```cpp
AudioDecoder decoder({.output_sample_rate = 16000});
decoder.open("audio.mp3");

auto samples = decoder.get_samples();  // vector<vector<float>>
std::cout << "Channels: " << samples.size() << std::endl;
std::cout << "Samples: " << samples[0].size() << std::endl;
```

#### Frame-by-Frame Decoding
```cpp
AudioDecoder decoder;
decoder.open("audio.mp3");

while (auto frame = decoder.read()) {
    // frame.data[channel][sample]
    for (int c = 0; c < frame.num_channels; c++) {
        process(frame.data[c], frame.num_samples);
    }
}
```

#### Streaming Decode (Push-based)
```cpp
AudioStreamOptions opts;
opts.input_format = "s16le";
opts.input_sample_rate = 48000;
opts.input_channels = 2;

AudioDecoder decoder(opts);
decoder.push(raw_bytes, size);  // Auto-initializes on first call

auto samples = decoder.get_samples(); // Decode all buffered data
// Or frame-by-frame:
while (auto frame = decoder.read()) {
    // Process decoded audio...
}
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
| `load(source)` | `Metadata` | Load file, URL, `pathlib.Path`, or bytes-like input |
| `decoder(data)` | `ndarray` | Push bytes-like data and decode (streaming) |
| `read()` | `ndarray` | Decode next frame |
| `get_samples()` | `ndarray` | Decode all available samples |
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
meta = decoder.load("speech.wav")
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
    samples = decoder(data)  # Push & get samples in one call
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
| `load(source)` | `Metadata`                 | Load file, URL, or device name. Returns metadata. |
| `push(buffer)` | `void`                     | Push raw encoded bytes for streaming.             |
| `read()`       | `Float32Array[]` \| `null` | Decode next frame. Returns array of channel data. |
| `getSamples()` | `Float32Array[]`           | Decode all available samples at once.             |
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
const meta = decoder.load("audio.wav");

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
    decoder.push(chunk);

    // Get all samples decoded from this chunk
    const samples = decoder.getSamples();
    if (samples.length > 0) {
        processAudio(samples);
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
    decoder.open("audio.mp3");
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
