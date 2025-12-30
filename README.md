# avioflow

A C++20 audio decoder library powered by FFmpeg 7.

## Features

- Decode audio from files, URLs, memory buffers
- Automatic resampling to float planar format (FLTP)
- Optional sample rate and channel count conversion
- Simple `FrameOutput` interface for zero-copy access

## Requirements

- Windows 10/11
- Visual Studio 2022+ (with C++20 support)
- CMake 3.20+
- Ninja (optional, for faster builds)

## Build

Run the build script from PowerShell:

```powershell
.\build.ps1
```

This will:
1. Download FFmpeg 7.1 shared libraries automatically
2. Configure and build the project
3. Output binaries to `build/`

## Test

### Basic Test

```powershell
.\build\avioflow_test.exe <audio_file>
```

Example:
```powershell
.\build\avioflow_test.exe .\TownTheme.mp3
```

### Unit Tests

```powershell
.\build\avioflow_load_test.exe
.\build\avioflow_resample_test.exe
```

Use `--skip-network` to skip URL-based tests:
```powershell
.\build\avioflow_load_test.exe --skip-network
```

## Usage

```cpp
#include "single-stream-decoder.h"

avioflow::SingleStreamDecoder decoder;
decoder.open("audio.mp3");

const auto& meta = decoder.get_metadata();
std::cout << "Sample Rate: " << meta.sample_rate << " Hz\n";
std::cout << "Channels: " << meta.num_channels << "\n";

while (decoder.has_more())
{
    auto frame = decoder.decode_next();
    if (frame.data == nullptr)
        break;

    // frame.data: pointer to planar float data (FLTP format)
    // frame.num_samples: samples per channel
    // frame.num_channels: number of channels

    const float* channel0 = reinterpret_cast<const float*>(frame.data);
    // Process audio data...
}
```

## API

### SingleStreamDecoder

| Method | Description |
|--------|-------------|
| `open(path)` | Open audio file, URL, or device |
| `open(data, size)` | Open from memory buffer |
| `decode_next()` | Decode next frame, returns `FrameOutput` |
| `has_more()` | Check if more frames available |
| `get_metadata()` | Get audio metadata (sample rate, channels, duration) |

### FrameOutput

| Field | Type | Description |
|-------|------|-------------|
| `data` | `uint8_t*` | Pointer to planar float audio data |
| `size` | `size_t` | Size in bytes per channel |
| `num_channels` | `int` | Number of audio channels |
| `num_samples` | `int` | Samples per channel |

## License

This project uses FFmpeg (LGPL).
