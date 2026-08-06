# avioflow (Rust)

Rust bindings for [AvioFlow](https://github.com/lxp3/avioflow), a
high-performance audio library built on FFmpeg.

The crate wraps the same C++ core as the Python, Node.js, Java and WebAssembly
bindings, reached through the C ABI the core exports
(`avioflow/include/avioflow-c-api.h`). FFmpeg is linked statically,
so there is nothing to install or locate at runtime.

## Install

```toml
[dependencies]
avioflow = "0.7"
```

Building the crate compiles the native core, so the host needs a C++17 compiler
and CMake 3.20 or newer. The first build downloads a prebuilt FFmpeg package and
takes a few minutes; later builds are cached.

## Conventions

- **Sample layout** is always planar float: `samples[channel][sample]`, with every
  channel the same length. A ragged input is rejected rather than truncated.
- **Fallible calls return `Result<T, Error>`.** [`Error::kind`] classifies the
  failure and [`Error::message`] carries the text from the native layer.
- **Handles are `Send` but not `Sync`.** Move a decoder, encoder or resampler
  between threads freely; sharing one across threads needs external
  synchronization.
- **Options use builders.** Unset fields keep the source format rather than
  applying a default.

## Decoding

### `AudioDecoder`

| Method | Purpose |
| ------ | ------- |
| `new(&StreamOptions) -> Result<Self>` | Create a decoder. |
| `load_file(&str) -> Result<Metadata>` | Open a file path, URL or device. |
| `load_buffer(&[u8]) -> Result<Metadata>` | Open complete file bytes in memory. |
| `feed(&[u8]) -> Result<()>` | Push encoded bytes for streaming decode. |
| `flush() -> Result<()>` | Mark streaming input complete. |
| `get_samples() -> Result<Vec<Vec<f32>>>` | Decode everything remaining. |
| `get_samples_range(f64, Option<f64>) -> Result<Vec<Vec<f32>>>` | Decode `[start, stop)` in seconds. |
| `get_frame() -> Result<Option<Frame<'_>>>` | Decode one frame without copying. |
| `is_finished() -> Result<bool>` | Whether the stream is exhausted. |
| `metadata() -> Result<Metadata>` | Current stream metadata. |

### `StreamOptions`

| Builder | Effect when unset |
| ------- | ----------------- |
| `output_sample_rate(i32)` | Preserves the source rate. |
| `output_num_channels(i32)` | Preserves the source channel count. |
| `input_sample_rate(i32)` | Required for raw PCM streaming. |
| `input_channels(i32)` | Required for raw PCM streaming. |
| `input_format(&str)` | Required to use `feed`. |

### Offline decoding

```rust,no_run
use avioflow::{AudioDecoder, StreamOptions};

# fn main() -> Result<(), avioflow::Error> {
let mut decoder = AudioDecoder::new(&StreamOptions::new().output_sample_rate(16000))?;
let metadata = decoder.load_file("audio.mp3")?;
let samples = decoder.get_samples()?;

println!("{:.1}s, {} Hz", metadata.duration, metadata.sample_rate);
println!("{} channels x {} samples", samples.len(), samples[0].len());
# Ok(())
# }
```

`load_file` reports the *source* stream. The resampler is not configured until
the first frame is decoded, so when `output_sample_rate` is set, the new rate
appears in `metadata()` after decoding rather than in the value `load_file`
returns.

### Time-range decoding

```rust,no_run
# use avioflow::{AudioDecoder, StreamOptions};
# fn main() -> Result<(), avioflow::Error> {
# let mut decoder = AudioDecoder::new(&StreamOptions::new())?;
# decoder.load_file("audio.mp3")?;
// Seconds 10.3 through 20.3, exclusive of the end
let window = decoder.get_samples_range(10.3, Some(20.3))?;

// From 30s to the end
let tail = decoder.get_samples_range(30.0, None)?;
# Ok(())
# }
```

Each call seeks independently, so one decoder can serve many ranges. Range
decoding requires offline mode; in stream mode only `(0.0, None)` is valid.

### Frame-by-frame, zero-copy

```rust,no_run
# use avioflow::{AudioDecoder, StreamOptions};
# fn main() -> Result<(), avioflow::Error> {
# let mut decoder = AudioDecoder::new(&StreamOptions::new())?;
# decoder.load_file("audio.mp3")?;
let mut total = 0;
while let Some(frame) = decoder.get_frame()? {
    total += frame.num_samples();
    if let Some(left) = frame.channel(0) {
        // `left` borrows decoder-owned memory; copy it to keep it
        let _peak = left.iter().fold(0.0f32, |a, s| a.max(s.abs()));
    }
}
# Ok(())
# }
```

`Frame` borrows the decoder, so the compiler prevents holding a frame across the
next decode call that would invalidate its buffers.

### Streaming decode

```rust,no_run
use avioflow::{AudioDecoder, StreamOptions};

# fn main() -> Result<(), avioflow::Error> {
# let chunks: Vec<Vec<u8>> = Vec::new();
let mut decoder = AudioDecoder::new(&StreamOptions::new().input_format("mp3"))?;

for chunk in &chunks {
    decoder.feed(chunk)?;
    while let Some(frame) = decoder.get_frame()? {
        let _ = frame.num_samples();
    }
}

decoder.flush()?; // then drain the remaining frames
while let Some(frame) = decoder.get_frame()? {
    let _ = frame.num_samples();
}
# Ok(())
# }
```

For raw PCM input, also set `input_sample_rate` and `input_channels`.

## Resampling

Two entry points: [`resample`] for a buffer held in full, [`AudioResampler`] for
audio arriving in chunks.

### One-shot

```rust
use avioflow::resample;

# fn main() -> Result<(), avioflow::Error> {
# let samples: Vec<Vec<f32>> = vec![vec![0.0; 44100], vec![0.0; 44100]];
let downsampled = resample(&samples, 44100, 16000, None)?;
let mono = resample(&samples, 44100, 16000, Some(1))?;
# Ok(())
# }
```

### Chunked

```rust
use avioflow::{AudioResampler, ResampleOptions};

# fn main() -> Result<(), avioflow::Error> {
# let chunks: Vec<Vec<Vec<f32>>> = Vec::new();
let mut resampler = AudioResampler::new(&ResampleOptions::new(44100, 16000))?;
let mut output: Vec<Vec<f32>> = Vec::new();

for chunk in &chunks {
    append(&mut output, resampler.process(chunk)?);
}
append(&mut output, resampler.flush()?);

fn append(output: &mut Vec<Vec<f32>>, part: Vec<Vec<f32>>) {
    if output.is_empty() {
        output.resize(part.len(), Vec::new());
    }
    for (channel, data) in output.iter_mut().zip(part) {
        channel.extend(data);
    }
}
# Ok(())
# }
```

`flush()` is not optional. The resampler holds back the last few milliseconds to
keep filter continuity, and skipping the flush discards them. Filter state
carries across `process` calls, so chunked output matches a one-shot conversion
sample for sample — calling `resample` per chunk instead would introduce a
discontinuity at every boundary.

`output_num_channels()` returns 0 until the first `process` call reveals the
input channel count. The channel count must not change between calls.

## Encoding

### One-shot

```rust,no_run
use avioflow::{save_audio, WriteOptions};

# fn main() -> Result<(), avioflow::Error> {
# let samples: Vec<Vec<f32>> = vec![vec![0.0; 16000]];
save_audio("out.wav", &samples, &WriteOptions::new()
    .container_format("wav")
    .codec_name("pcm_s16le")
    .sample_rate(16000)
    .num_channels(1))?;
# Ok(())
# }
```

### Reusable encoder

```rust,no_run
use avioflow::{AudioEncoder, WriteOptions};

# fn main() -> Result<(), avioflow::Error> {
# let parts: Vec<Vec<Vec<f32>>> = Vec::new();
let mut encoder = AudioEncoder::new(&WriteOptions::new()
    .container_format("flac")
    .codec_name("flac")
    .sample_rate(44100))?;

for (index, part) in parts.iter().enumerate() {
    encoder.save(&format!("part{index}.flac"), part)?;
}
# Ok(())
# }
```

### `WriteOptions`

| Builder | Common values |
| ------- | ------------- |
| `codec_name(&str)` | `"pcm_s16le"`, `"flac"`, `"aac"`, `"libmp3lame"`, `"libopus"` |
| `container_format(&str)` | `"wav"`, `"flac"`, `"mp4"`, `"ogg"`, `"adts"` |
| `sample_format(&str)` | `"s16"`, `"s32"`, `"flt"`, `"fltp"` |
| `sample_rate(i32)` | 8000, 16000, 44100, 48000 |
| `num_channels(i32)` | 1, 2 |
| `bit_rate(i64)` | 128000, 192000, 320000 |
| `overwrite(bool)` | Defaults to `true` |

Unset fields are inferred from the container and the input samples.

## Info and diagnostics

```rust
# fn main() -> Result<(), avioflow::Error> {
let decoders = avioflow::supported_decoders()?;       // "mp3", "aac", ...
let encoders = avioflow::supported_encoders()?;       // "pcm_s16le", "flac", ...
let demuxers = avioflow::supported_input_formats()?;  // "mp3", "wav", ...
let muxers = avioflow::supported_output_formats()?;   // "wav", "flac", ...

for device in avioflow::list_audio_devices()? {
    println!("{} ({}) output={}", device.name, device.description, device.is_output);
}

avioflow::set_log_level(Some("warning")); // or None for the default
# Ok(())
# }
```

Metadata without a full decode:

```rust,no_run
# use avioflow::{AudioDecoder, StreamOptions};
# fn main() -> Result<(), avioflow::Error> {
let mut decoder = AudioDecoder::new(&StreamOptions::new())?;
let metadata = decoder.load_file("audio.mp3")?;
println!("{} / {} @ {} Hz, {:.1}s",
    metadata.container, metadata.codec, metadata.sample_rate, metadata.duration);
# Ok(())
# }
```

`Metadata` also carries `num_samples`, `num_channels`, `bit_rate` and
`sample_format`. Note `codec` is the FFmpeg *decoder* name, so an MP3 file
reports `"mp3float"` while `container` reports `"mp3"`.

## Error handling

```rust
use avioflow::{AudioDecoder, ErrorKind, StreamOptions};

# fn main() -> Result<(), avioflow::Error> {
let mut decoder = AudioDecoder::new(&StreamOptions::new())?;
match decoder.load_file("missing.mp3") {
    Ok(metadata) => println!("{} Hz", metadata.sample_rate),
    Err(error) => match error.kind() {
        ErrorKind::InvalidArgument => eprintln!("bad argument: {error}"),
        ErrorKind::Runtime => eprintln!("could not decode: {error}"),
        ErrorKind::InvalidString => eprintln!("path contained a NUL byte"),
        ErrorKind::Unknown => eprintln!("unclassified failure: {error}"),
    },
}
# Ok(())
# }
```

`Error` implements `std::error::Error`, so it works with `?` and `Box<dyn Error>`.

## Building from a repository checkout

```bash
cd rust
cargo test
```

`build.rs` configures the repository root with static linkage. To pass extra CMake arguments — for example to reuse an already
downloaded FFmpeg package:

```bash
AVIOFLOW_CMAKE_ARGS="-DFETCHCONTENT_SOURCE_DIR_FFMPEG_BIN=/path/to/ffmpeg" cargo test
```

The crate version is derived from the repository's `version.txt`; run
`python3 scripts/sync_version.py` after changing it rather than editing
`Cargo.toml` directly.

## Publishing

The crate compiles the C++ core, but Cargo can only publish files inside
`rust/`, so the native sources must be staged there first:

```bash
python3 scripts/vendor_rust_sources.py   # copies sources into rust/native/
cargo publish --manifest-path rust/Cargo.toml
```

`rust/native/` is generated and gitignored; `build.rs` uses it when present and
falls back to the repository root otherwise, so a checkout needs no vendoring to
run `cargo test`.

Verify the package builds as a consumer would before publishing, since a
publish is irreversible — a version can be yanked but never replaced:

```bash
python3 scripts/vendor_rust_sources.py
cargo package --manifest-path rust/Cargo.toml
tar xzf rust/target/package/avioflow-*.crate -C /tmp
cd /tmp/avioflow-* && cargo build
```

CI runs this same check on every tag.

## Platform support

Verified on Linux x86_64. The macOS and Windows link configuration is
implemented but not yet exercised in CI.

`avioflow/core/compat/glibc-finite-compat.c` supplies the `__*_finite` math
symbols that glibc 2.31 removed, which the bundled libvorbis still refers to.
They are compiled in whenever FFmpeg is linked statically on Linux, which is
what the crate does; the other bindings link shared FFmpeg, which does not refer
to them.

## License

MIT
