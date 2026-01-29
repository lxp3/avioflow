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

### Node.js
```bash
npm install avioflow
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

### ESM Import
```javascript
import avioflow from 'avioflow';
```

### `AudioDecoder`

```javascript
// Constructor with options object
const decoder = new avioflow.AudioDecoder({
    outputSampleRate: 16000,    // Optional
    outputNumChannels: 1,       // Optional
    inputFormat: 's16le',       // For streaming
    inputSampleRate: 48000,     // For streaming
    inputChannels: 2            // For streaming
});
```

#### Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `load(source)` | `Metadata` | Load file or URL |
| `push(buffer)` | `void` | Push raw bytes for streaming |
| `decodeNext()` | `Float32Array[]` or `null` | Decode next frame |
| `getMetadata()` | `Metadata` | Get audio metadata |
| `isFinished()` | `boolean` | Check if EOF |

### Examples

#### File Decoding
```javascript
const decoder = new avioflow.AudioDecoder({ outputSampleRate: 16000 });
const meta = decoder.load("audio.mp3");
console.log(`Duration: ${meta.duration}s, Channels: ${meta.numChannels}`);

while (!decoder.isFinished()) {
    const frame = decoder.decodeNext();
    if (frame) {
        // frame is array of Float32Array (one per channel)
        console.log(`Decoded ${frame[0].length} samples`);
    }
}
```

#### Streaming Decode
```javascript
const decoder = new avioflow.AudioDecoder({
    inputFormat: 's16le',
    inputSampleRate: 48000,
    inputChannels: 2
});

socket.on('data', (chunk) => {
    decoder.push(chunk);
    let frame;
    while ((frame = decoder.decodeNext()) !== null) {
        processAudio(frame);
    }
});
```

#### Device Discovery
```javascript
const devices = avioflow.listAudioDevices();
devices.forEach(dev => {
    console.log(`${dev.isOutput ? 'Output' : 'Input'}: ${dev.name}`);
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
