# avioflow (Go)

Go bindings for [AvioFlow](https://github.com/lxp3/avioflow), a
high-performance audio library built on FFmpeg.

The package wraps the same C++ core as the Python, Node.js, Java, Rust and
WebAssembly bindings, reached through the C ABI the core exports
(`avioflow/include/avioflow-c-api.h`).

## Install

```bash
go get github.com/lxp3/avioflow/go
```

Unlike the Rust crate, cgo cannot compile the native core during `go build`, so
the C library must be installed first:

```bash
git clone https://github.com/lxp3/avioflow
cd avioflow
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build build
sudo cmake --install build          # or --prefix ~/.local
```

That installs `lib/pkgconfig/avioflow.pc`, which is how the package finds the
library. If you installed to a prefix pkg-config does not search by default:

```bash
export PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig
```

`CGO_CFLAGS` and `CGO_LDFLAGS` override the pkg-config result if you need to
point at a build tree directly.

Building the C library needs a C++17 compiler and CMake 3.20 or newer. FFmpeg is
fetched automatically during the CMake configure step. A static build (as above)
means the resulting Go binaries carry no runtime dependency on avioflow or FFmpeg.

## Conventions

- **Sample layout** is always planar float32: `samples[channel][sample]`, with
  every channel the same length. A ragged input is rejected rather than truncated.
- **Errors** are `*Error` values carrying the message from the native layer, and
  match the sentinels via `errors.Is`:

  ```go
  if errors.Is(err, avioflow.ErrInvalidArgument) { ... }
  ```

- **Handles must be closed.** `Decoder`, `Encoder` and `Resampler` hold native
  memory; `Close` is idempotent and safe to call twice. A finalizer releases a
  forgotten handle, but relying on it is unsupported.
- **Optional options are pointers.** The C ABI distinguishes "unset" from any
  value, so use `avioflow.Int()` rather than a sentinel:
  `&StreamOptions{OutputSampleRate: avioflow.Int(16000)}`.

## Quick start

```go
package main

import (
	"fmt"
	"log"

	avioflow "github.com/lxp3/avioflow/go"
)

func main() {
	dec, err := avioflow.NewDecoder(&avioflow.StreamOptions{
		OutputSampleRate: avioflow.Int(16000),
	})
	if err != nil {
		log.Fatal(err)
	}
	defer dec.Close()

	meta, err := dec.LoadFile("audio.mp3")
	if err != nil {
		log.Fatal(err)
	}

	samples, err := dec.GetSamples()
	if err != nil {
		log.Fatal(err)
	}

	fmt.Printf("%.1fs, %d Hz, %d channels\n", meta.Duration, meta.SampleRate, meta.NumChannels)
	fmt.Printf("%d channels x %d samples\n", len(samples), len(samples[0]))
}
```

## Package functions

| Function | Returns | Description |
| -------- | ------- | ----------- |
| `SaveAudio(path, samples, options)` | `error` | Write samples to a file. |
| `Resample(samples, inRate, outRate, outChannels)` | `([][]float32, error)` | Resample a complete buffer. |
| `SetLogLevel(level)` | | FFmpeg verbosity; `""` restores the default. |
| `SupportedDecoders()` | `([]string, error)` | Available decoders, e.g. `"mp3"`. |
| `SupportedEncoders()` | `([]string, error)` | Available encoders, e.g. `"pcm_s16le"`. |
| `SupportedInputFormats()` | `([]string, error)` | Available demuxers. |
| `SupportedOutputFormats()` | `([]string, error)` | Available muxers. |
| `ListAudioDevices()` | `([]DeviceInfo, error)` | Enumerate audio devices. |
| `Int(v)`, `Int64(v)` | pointer | Helpers for optional option fields. |

## Decoding

### `Decoder`

| Method | Description |
| ------ | ----------- |
| `NewDecoder(*StreamOptions) (*Decoder, error)` | Create a decoder; nil options keep the source format. |
| `LoadFile(string) (Metadata, error)` | Open a file path, URL or device. |
| `LoadBuffer([]byte) (Metadata, error)` | Open complete file bytes in memory. |
| `Feed([]byte) error` | Push encoded bytes for streaming decode. |
| `Flush() error` | Mark streaming input complete. |
| `GetSamples() ([][]float32, error)` | Decode everything remaining. |
| `GetSamplesRange(start, stop float64)` | Decode `[start, stop)` in seconds. |
| `GetSamplesFrom(start float64)` | Decode from `start` to the end. |
| `GetFrame() ([][]float32, error)` | Decode one frame; nil at end of stream. |
| `IsFinished() (bool, error)` | Whether the stream is exhausted. |
| `Metadata() (Metadata, error)` | Current stream metadata. |
| `Close() error` | Release the native handle. |

### `StreamOptions`

| Field | Effect when unset |
| ----- | ----------------- |
| `OutputSampleRate *int32` | Preserves the source rate. |
| `OutputNumChannels *int32` | Preserves the source channel count. |
| `InputSampleRate *int32` | Required for raw PCM streaming. |
| `InputChannels *int32` | Required for raw PCM streaming. |
| `InputFormat string` | Required to use `Feed`. |

### Offline decoding

```go
dec, err := avioflow.NewDecoder(&avioflow.StreamOptions{
	OutputSampleRate: avioflow.Int(16000),
})
if err != nil {
	return err
}
defer dec.Close()

meta, err := dec.LoadFile("audio.mp3")
samples, err := dec.GetSamples()
```

`LoadFile` reports the *source* stream. The resampler is not configured until the
first frame is decoded, so when `OutputSampleRate` is set the new rate appears in
`Metadata()` after decoding rather than in what `LoadFile` returns.

### Time-range decoding

```go
// Seconds 10.3 through 20.3, exclusive of the end
window, err := dec.GetSamplesRange(10.3, 20.3)

// From 30s to the end
tail, err := dec.GetSamplesFrom(30.0)
```

Each call seeks independently, so one decoder can serve many ranges. Range
decoding requires offline mode; in stream mode use `GetSamples`, which drains
whatever is currently buffered.

### Frame-by-frame

```go
total := 0
for {
	frame, err := dec.GetFrame()
	if err != nil {
		return err
	}
	if frame == nil {
		break
	}
	total += len(frame[0])
}
```

The samples are copied out of the decoder's buffers. The C ABI exposes those
buffers directly for zero-copy access, but Go cannot express a lifetime tied to
the next decode call, so a borrowed slice would become a use-after-free. Use the C
or Rust API if zero-copy decoding matters.

### Streaming decode

```go
dec, err := avioflow.NewDecoder(&avioflow.StreamOptions{InputFormat: "mp3"})
if err != nil {
	return err
}
defer dec.Close()

for _, chunk := range chunks {
	if err := dec.Feed(chunk); err != nil {
		return err
	}
	samples, err := dec.GetSamples()
	if err != nil {
		return err
	}
	process(samples)
}

if err := dec.Flush(); err != nil {   // then drain the remainder
	return err
}
remaining, err := dec.GetSamples()
```

`Flush` discards nothing. It marks input complete so buffered bytes and
codec-delayed frames can still be drained. For raw PCM input, also set
`InputSampleRate` and `InputChannels`.

## Encoding

### One-shot

```go
err := avioflow.SaveAudio("out.wav", samples, &avioflow.WriteOptions{
	ContainerFormat: "wav",
	CodecName:       "pcm_s16le",
	SampleRate:      avioflow.Int(16000),
	NumChannels:     avioflow.Int(1),
})
```

### Reusable encoder

```go
enc, err := avioflow.NewEncoder(&avioflow.WriteOptions{
	ContainerFormat: "flac",
	CodecName:       "flac",
	SampleRate:      avioflow.Int(44100),
})
if err != nil {
	return err
}
defer enc.Close()

for i, part := range parts {
	if err := enc.Save(fmt.Sprintf("part%d.flac", i), part); err != nil {
		return err
	}
}
```

### `WriteOptions`

| Field | Common values |
| ----- | ------------- |
| `CodecName` | `"pcm_s16le"`, `"flac"`, `"aac"`, `"libmp3lame"`, `"libopus"` |
| `ContainerFormat` | `"wav"`, `"flac"`, `"mp4"`, `"ogg"`, `"adts"` |
| `SampleFormat` | `"s16"`, `"s32"`, `"flt"`, `"fltp"` |
| `SampleRate *int32` | 8000, 16000, 44100, 48000 |
| `NumChannels *int32` | 1, 2 |
| `BitRate *int64` | 128000, 192000, 320000 |
| `NoOverwrite bool` | Opt out of replacing an existing file |

Unset fields are inferred from the container and the input samples. `NoOverwrite`
is an opt-out so the zero value matches the C++ default of overwriting.

## Resampling

Two entry points: `Resample` for a buffer held in full, `Resampler` for audio
arriving in chunks. Both work on any `[][]float32`, not only avioflow output.

To resample *while decoding*, set `OutputSampleRate` on `StreamOptions` instead;
that avoids a second pass over the samples.

### One-shot

```go
downsampled, err := avioflow.Resample(samples, 44100, 16000, nil)
mono, err := avioflow.Resample(samples, 44100, 16000, avioflow.Int(1))
```

### Chunked

```go
r, err := avioflow.NewResampler(&avioflow.ResampleOptions{
	InputSampleRate:  44100,
	OutputSampleRate: 16000,
})
if err != nil {
	return err
}
defer r.Close()

var out [][]float32
appendPart := func(part [][]float32) {
	if out == nil {
		out = make([][]float32, len(part))
	}
	for c := range part {
		out[c] = append(out[c], part[c]...)
	}
}

for _, chunk := range chunks {
	part, err := r.Process(chunk)
	if err != nil {
		return err
	}
	appendPart(part)
}

tail, err := r.Flush()   // else the tail is lost
if err != nil {
	return err
}
appendPart(tail)
```

`Flush` is not optional. The resampler holds back the last few milliseconds to
keep filter continuity, and skipping the flush discards them. Filter state carries
across `Process` calls, so chunked output matches a one-shot conversion sample for
sample — calling `Resample` per chunk instead would introduce a discontinuity at
every boundary.

`OutputNumChannels` returns 0 until the first `Process` call reveals the input
channel count. The channel count must not change between calls.

## Metadata

| Field | Type | Description |
| ----- | ---- | ----------- |
| `Duration` | `float64` | Seconds; 0 for live streams. |
| `NumSamples` | `int64` | Samples per channel; updated at EOF for streams. |
| `SampleRate` | `int32` | Hz. |
| `NumChannels` | `int32` | 1 for mono, 2 for stereo. |
| `BitRate` | `int64` | Bits per second. |
| `SampleFormat` | `string` | Source format, e.g. `"fltp"`. |
| `Codec` | `string` | Decoder name, e.g. `"mp3float"`. |
| `Container` | `string` | Container format, e.g. `"mp3"`. |

Note `Codec` is the FFmpeg *decoder* name, so an MP3 file reports `"mp3float"`
while `Container` reports `"mp3"`.

## Devices and diagnostics

```go
devices, err := avioflow.ListAudioDevices()
for _, d := range devices {
	fmt.Printf("%s: %s (output=%v)\n", d.Name, d.Description, d.IsOutput)
}

// Open a device by passing its name as the source
dec, _ := avioflow.NewDecoder(nil)
defer dec.Close()
dec.LoadFile("audio=Microphone")

avioflow.SetLogLevel(avioflow.LogDebug)   // or "" for the default
```

## Concurrency

Each handle serializes its own calls with a mutex, so concurrent use will not
corrupt native state. Decoding is inherently sequential, though, so interleaving
calls on one `Decoder` from several goroutines is a logic error. Give each
goroutine its own handle.

CI runs the test suite under the race detector and under strict cgo pointer
checking, which catches Go pointers written into C memory — a mistake the default
checks miss and that corrupts memory only intermittently:

```bash
go test -race ./...
GOEXPERIMENT=cgocheck2 go test -count=1 ./...   # Go 1.21+
GODEBUG=cgocheck=2 go test -count=1 ./...       # Go 1.20 and earlier
```

The flag moved from a runtime setting to a build-time experiment in Go 1.21, and
the old form is a fatal error on newer toolchains.

## Running the tests

```bash
cmake -S . -B build -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=$PWD/build/prefix
cmake --build build && cmake --install build

cd go
PKG_CONFIG_PATH=$PWD/../build/prefix/lib/pkgconfig go test ./...
```

## Platform support

Verified on Linux x86_64. macOS should work through the same pkg-config path.
Windows is untested: pkg-config is uncommon there, so `CGO_CFLAGS` and
`CGO_LDFLAGS` are likely needed.

## License

MIT
