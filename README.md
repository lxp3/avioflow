# AvioFlow

High-performance audio decoding library powered by FFmpeg with C++, Python, and Node.js bindings.

## Features

- **Multi-format Support**: MP3, WAV, AAC, FLAC, Opus, and all FFmpeg-supported formats
- **Flexible Input**: Files, URLs, memory buffers, and real-time streams
- **Hardware Capture**: WASAPI loopback (system audio) and DirectShow (microphones)
- **Resampling**: Built-in sample rate and channel conversion
- **Zero-copy API**: Direct buffer access via `FrameData` for maximum performance
- **Cross-platform**: Windows, Linux, macOS

---

## Installation

### Python
```bash
pip install avioflow
```


### C++ (CMake)
```cmake
find_package(avioflow REQUIRED)
target_link_libraries(your_target avioflow::avioflow)
```

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
options.output_num_channels = 1;       // Target channels
options.input_format = "s16le";        // For streaming: source format
options.input_sample_rate = 48000;     // For streaming: source rate
options.input_channels = 2;            // For streaming: source channels

AudioDecoder decoder(options);
```

#### Methods

| Method | Description |
|--------|-------------|
| `open(source)` | Open file path, URL, or device |
| `push(data, size)` | Push raw bytes for streaming decode |
| `decode_next()` | Decode next frame, returns `FrameData` |
| `get_all_samples()` | Decode all and return `vector<vector<float>>` |
| `get_metadata()` | Get audio metadata |
| `is_finished()` | Check if EOF reached |

#### `FrameData`

Zero-copy frame data structure returned by `decode_next()`.

```cpp
struct FrameData {
    float** data;        // Planar channel pointers: data[channel][sample]
    int num_channels;    // Number of channels
    int num_samples;     // Samples per channel
    
    operator bool();     // True if valid data
};
```

> ⚠️ **Warning**: `FrameData.data` points to internal buffer, valid only until next `decode_next()` call.

### Examples

#### File Decoding (Offline)
```cpp
AudioDecoder decoder({.output_sample_rate = 16000});
decoder.open("audio.mp3");

auto samples = decoder.get_all_samples();  // vector<vector<float>>
std::cout << "Channels: " << samples.size() << std::endl;
std::cout << "Samples: " << samples[0].size() << std::endl;
```

#### Frame-by-Frame Decoding
```cpp
AudioDecoder decoder;
decoder.open("audio.mp3");

while (auto frame = decoder.decode_next()) {
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

while (auto frame = decoder.decode_next()) {
    // Process decoded audio...
}
```

#### WASAPI Loopback Capture
```cpp
AudioDecoder decoder;
decoder.open("wasapi_loopback");

while (auto frame = decoder.decode_next()) {
    // Capture system audio in real-time
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
    output_num_channels=1,       # Optional: target channels
    input_format="s16le",        # For streaming: source format
    input_sample_rate=48000,     # For streaming: source rate
    input_channels=2             # For streaming: source channels
)
```

#### Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `load(source)` | `Metadata` | Load file, URL, or `pathlib.Path` |
| `decoder(bytes)` | `ndarray` | Push bytes and decode (streaming) |
| `get_all_samples()` | `ndarray` | Decode entire source |
| `is_finished()` | `bool` | Check if EOF |

#### `Metadata`

```python
meta = decoder.load("audio.mp3")
print(f"Duration: {meta.duration}s")
print(f"Sample Rate: {meta.sample_rate}Hz")
print(f"Channels: {meta.num_channels}")
print(f"Codec: {meta.codec}")
print(f"Container: {meta.container}")
print(f"Bit Rate: {meta.bit_rate}bps")
```

### Examples

#### File Decoding
```python
decoder = avioflow.AudioDecoder(output_sample_rate=16000)
meta = decoder.load("speech.wav")
samples = decoder.get_all_samples()  # numpy array (channels, samples)
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
    samples = decoder(data)  # Call decoder directly
    if samples.size > 0:
        process_audio(samples)
```

#### Device Discovery
```python
devices = avioflow.DeviceManager.list_audio_devices()
for dev in devices:
    print(f"{dev.name}: {dev.description}")
    # dev.is_output: True for output/loopback devices
```

### Logging
```python
avioflow.set_log_level("debug")  # quiet, error, warning, info, debug, trace
```

---

## Node.js API

### Compatibility

Avioflow provides pre-built native binaries for major platforms and runtimes, ensuring zero-config installation in most environments.

| Runtime | Version / ABI | Support |
|---------|---------------|---------|
| **Node.js** | 16, 18, 20, 22+ | ✅ Native (N-API) |
| **Electron** | 28, 30, 32, 34 | ✅ VS Code 1.85 ~ 1.96+ |
| **Electron** | 37, 38, 39 | ✅ Latest & Future Proof |
| **Architectures** | x64 | ✅ Linux, Windows |

The library automatically detects whether it's running in standard Node.js or an Electron environment (like a VS Code extension) and loads the optimized binary accordingly.

### Installation

```bash
npm install avioflow
```

### ESM Import
```javascript
import avioflow from 'avioflow';
```

### Module-level Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `load(path, options?)` | `{metadata, samples}` | **Convenience**: Opens, decodes all samples, and returns both in one call. |
| `listAudioDevices()` | `DeviceInfo[]` | List available system audio devices. |
| `setLogLevel(level)` | `void` | Set FFmpeg log level ("quiet", "info", "debug", etc.). |

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

| Method | Returns | Description |
|--------|---------|-------------|
| `load(source)` | `Metadata` | Load file, URL, or device name. Returns metadata. |
| `push(buffer)` | `void` | Push raw encoded bytes for streaming. |
| `decodeNext()` | `Float32Array[]` \| `null` | Decode next frame. Returns array of channel data. |
| `getAllSamples()` | `Float32Array[]` | Decode all remaining samples at once. |
| `isFinished()` | `boolean` | Check if end of stream reached. |

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
const allSamples = decoder.getAllSamples(); 
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
    let frame;
    // Extract all available frames from the pushed chunk
    while ((frame = decoder.decodeNext()) !== null) {
        processAudio(frame); // frame is Float32Array[]
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

## Build from Source

### Prerequisites
- CMake 3.20+
- Visual Studio 2022+ (Windows) or GCC 11+ (Linux)
- Python 3.8+ with pybind11 (for Python bindings)
- Node.js 16+ (for Node.js bindings)

### Windows
```powershell
.\build.ps1
```

### Linux
```bash
cmake -B build -DENABLE_PYTHON=ON
cmake --build build --config Release
```

### Node.js Prebuild
```bash
npm run prebuild
```

---

## Input Format Reference

| Format | Description | Use Case |
|--------|-------------|----------|
| `s16le` | 16-bit signed PCM, little-endian | Raw audio, WebRTC |
| `s16be` | 16-bit signed PCM, big-endian | Network streams |
| `f32le` | 32-bit float PCM, little-endian | High-quality audio |
| `aac` | AAC with ADTS headers | Streaming AAC |
| `mp3` | MP3 frames | Streaming MP3 |
| `opus` | Opus packets | WebRTC, VoIP |
| `wav` | WAV container | File-based audio |

---

## License

MIT License
