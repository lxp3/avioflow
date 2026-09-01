# avioflow (Java)

Java bindings for [AvioFlow](https://github.com/lxp3/avioflow), a
high-performance audio library built on FFmpeg.

The library wraps the same C++ core as the Python, Node.js, Rust and WebAssembly
bindings, through a JNI layer. FFmpeg is bundled in the native classifier jar, so
there is nothing to install or locate at runtime.

## Install

Two artifacts are needed: the main API jar plus one native classifier for the
target platform.

Gradle:

```kotlin
dependencies {
    implementation("io.github.lxp3:avioflow:0.7.9")
    runtimeOnly("io.github.lxp3:avioflow:0.7.9:linux-x86_64")
}
```

Maven:

```xml
<dependency>
  <groupId>io.github.lxp3</groupId>
  <artifactId>avioflow</artifactId>
  <version>0.7.9</version>
</dependency>
<dependency>
  <groupId>io.github.lxp3</groupId>
  <artifactId>avioflow</artifactId>
  <version>0.7.9</version>
  <classifier>linux-x86_64</classifier>
  <scope>runtime</scope>
</dependency>
```

Native classifiers: `linux-x86_64`, `linux-aarch64`, `macos-x86_64`,
`macos-aarch64`, `windows-x86_64`, `windows-aarch64`.

To ship one build that runs anywhere, depend on every classifier you need at
runtime; the loader picks the matching one.

## Conventions

- **Samples are `float[][]`**, one array per channel: `samples[channel]` holds
  that channel's data, and every channel has the same length.
- **Failures throw `AvioflowException`**, carrying the message from the native
  layer.
- **`AudioDecoder` is `AutoCloseable`.** Use try-with-resources; the native
  handle is released on `close()`, not by the garbage collector.
- **Unset options preserve the source format.** Leaving a rate or channel count
  unset, or setting `-1`, keeps whatever the input has.

## Quick start

```java
import io.github.lxp3.avioflow.AudioDecoder;
import io.github.lxp3.avioflow.AudioStreamOptions;
import io.github.lxp3.avioflow.Metadata;

try (AudioDecoder decoder = new AudioDecoder(
        new AudioStreamOptions().outputSampleRate(16000))) {
    Metadata meta = decoder.loadFile("audio.mp3");
    float[][] samples = decoder.getSamples();

    System.out.printf("%.1fs at %d Hz%n", meta.duration, meta.sampleRate);
    System.out.printf("%d channels x %d samples%n", samples.length, samples[0].length);
}
```

## Static methods

| Method | Returns | Description |
| ------ | ------- | ----------- |
| `Avioflow.setLogLevel(String)` | `void` | FFmpeg verbosity. |
| `Avioflow.getSupportedDecoders()` | `String[]` | Available decoders, e.g. `"mp3"`. |
| `Avioflow.getSupportedEncoders()` | `String[]` | Available encoders, e.g. `"pcm_s16le"`. |
| `Avioflow.getSupportedInputFormats()` | `String[]` | Available demuxers. |
| `Avioflow.getSupportedOutputFormats()` | `String[]` | Available muxers. |
| `Avioflow.listAudioDevices()` | `DeviceInfo[]` | Enumerate audio devices. |
| `AudioEncoder.saveAudio(String, float[][], AudioWriteOptions)` | `void` | Write samples to a file. |
| `AudioResampler.resample(float[][], int, int)` | `float[][]` | Resample a complete buffer. |
| `AudioResampler.resample(float[][], int, int, int)` | `float[][]` | Same, also setting the channel count. |

## Decoding

### `AudioDecoder`

| Method | Returns | Description |
| ------ | ------- | ----------- |
| `loadFile(String)` | `Metadata` | Open a path, URL or device. |
| `loadBuffer(byte[])` | `Metadata` | Open complete file bytes in memory. |
| `feed(byte[])` | `void` | Push encoded bytes for streaming decode. |
| `flush()` | `void` | Mark streaming input complete. |
| `getSamples()` | `float[][]` | Decode everything remaining. |
| `getSamples(double, double)` | `float[][]` | Decode `[start, stop)` in seconds. |
| `getFrame()` | `float[][]` | Decode one frame; empty at end of stream. |
| `isFinished()` | `boolean` | Whether the stream is exhausted. |
| `getMetadata()` | `Metadata` | Current stream metadata. |
| `close()` | `void` | Release the native handle. |

Methods are `synchronized`, so one decoder can be shared between threads,
though calls serialize.

### `AudioStreamOptions`

| Builder | Effect when unset |
| ------- | ----------------- |
| `outputSampleRate(int)` | Preserves the source rate. |
| `outputNumChannels(int)` | Preserves the source channel count. |
| `inputSampleRate(int)` | Required for raw PCM streaming. |
| `inputChannels(int)` | Required for raw PCM streaming. |
| `inputFormat(String)` | Required to use `feed`. |

### Offline decoding

```java
try (AudioDecoder decoder = new AudioDecoder()) {
    Metadata meta = decoder.loadFile("audio.mp3");
    float[][] samples = decoder.getSamples();
}
```

`loadFile` reports the *source* stream. The resampler is not configured until
the first frame is decoded, so when `outputSampleRate` is set the new rate
appears in `getMetadata()` after decoding rather than in what `loadFile`
returns.

### Time-range decoding

```java
try (AudioDecoder decoder = new AudioDecoder()) {
    decoder.loadFile("audio.mp3");

    // Seconds 10.3 through 20.3, exclusive of the end
    float[][] window = decoder.getSamples(10.3, 20.3);

    // Another range from the same decoder; each call seeks independently
    float[][] later = decoder.getSamples(30.0, 40.0);
}
```

Range decoding requires offline mode. In stream mode use the no-argument
`getSamples()`, which drains whatever is currently buffered.

### Frame-by-frame

```java
try (AudioDecoder decoder = new AudioDecoder()) {
    decoder.loadFile("audio.mp3");

    long total = 0;
    float[][] frame;
    while ((frame = decoder.getFrame()).length > 0) {
        total += frame[0].length;
    }
}
```

`getFrame()` returns a zero-length array at end of stream rather than null.

### Streaming decode

```java
try (AudioDecoder decoder = new AudioDecoder(
        new AudioStreamOptions()
            .inputFormat("s16le")
            .inputSampleRate(48000)
            .inputChannels(2))) {

    byte[] chunk;
    while ((chunk = source.read()) != null) {
        decoder.feed(chunk);
        float[][] samples = decoder.getSamples();
        if (samples.length > 0) process(samples);
    }

    decoder.flush();                       // then drain the remainder
    float[][] remaining = decoder.getSamples();
}
```

`flush()` discards nothing. It marks input complete so buffered bytes and
codec-delayed frames can still be drained. For raw PCM input, also set
`inputSampleRate` and `inputChannels`.

## Encoding

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

### `AudioWriteOptions`

| Builder | Common values |
| ------- | ------------- |
| `codecName(String)` | `"pcm_s16le"`, `"flac"`, `"aac"`, `"libmp3lame"`, `"libopus"` |
| `containerFormat(String)` | `"wav"`, `"flac"`, `"mp4"`, `"ogg"`, `"adts"` |
| `sampleFormat(String)` | `"s16"`, `"s32"`, `"flt"`, `"fltp"` |
| `sampleRate(int)` | 8000, 16000, 44100, 48000 |
| `numChannels(int)` | 1, 2 |
| `bitRate(long)` | 128000, 192000, 320000 |
| `overwrite(boolean)` | Defaults to `true` |

Unset fields are inferred from the container and the input samples. A format
preset is also available:

```java
AudioWriteOptions options = AudioWriteOptions.format("wav").sampleRate(16000);
```

## Resampling

Two entry points: the static `AudioResampler.resample` for a buffer held in full,
and an `AudioResampler` instance for audio arriving in chunks. Both work on any
`float[][]`, not only avioflow output.

To resample *while decoding*, set `outputSampleRate` on `AudioStreamOptions`
instead; that avoids a second pass over the samples.

### One-shot

```java
float[][] downsampled = AudioResampler.resample(samples, 44100, 16000);
float[][] mono = AudioResampler.resample(samples, 44100, 16000, 1);
```

### Chunked

```java
try (AudioResampler resampler = new AudioResampler(
        new AudioResampleOptions(44100, 16000))) {
    for (float[][] chunk : chunks) {
        consume(resampler.process(chunk));
    }
    consume(resampler.flush());   // else the tail is lost
}
```

`flush()` is not optional. The resampler holds back the last few milliseconds to
keep filter continuity, and skipping the flush discards them. Filter state
carries across `process` calls, so chunked output matches a one-shot conversion
sample for sample — calling the static `resample` per chunk instead would
introduce a discontinuity at every boundary.

`outputNumChannels()` returns 0 until the first `process` call reveals the input
channel count. The channel count must not change between calls.

### `AudioResampleOptions`

| Builder | Notes |
| ------- | ----- |
| `inputSampleRate(int)` | Required, must be greater than zero. |
| `outputSampleRate(int)` | Required, must be greater than zero. |
| `outputNumChannels(int)` | Defaults to the input channel count. |

Both rates can also be passed to the constructor:
`new AudioResampleOptions(44100, 16000)`.

## Metadata

`Metadata` exposes public final fields:

| Field | Type | Description |
| ----- | ---- | ----------- |
| `duration` | `double` | Seconds; 0 for live streams. |
| `numSamples` | `long` | Samples per channel; updated at EOF for streams. |
| `sampleRate` | `int` | Hz. |
| `numChannels` | `int` | 1 for mono, 2 for stereo. |
| `sampleFormat` | `String` | Source format, e.g. `"fltp"`. |
| `codec` | `String` | Decoder name, e.g. `"mp3float"`. |
| `bitRate` | `long` | Bits per second. |
| `container` | `String` | Container format, e.g. `"mp3"`. |

Note `codec` is the FFmpeg *decoder* name, so an MP3 file reports
`"mp3float"` while `container` reports `"mp3"`.

## Devices and diagnostics

```java
for (DeviceInfo dev : Avioflow.listAudioDevices()) {
    System.out.printf("%s: %s (output=%b)%n", dev.name, dev.description, dev.isOutput);
}

// Open a device by passing its name as the source
try (AudioDecoder decoder = new AudioDecoder()) {
    decoder.loadFile("audio=Microphone");
}

Avioflow.setLogLevel("debug");   // quiet, error, warning, info, debug, trace
```

## Build from source

Building needs JDK 17+, a C++17 compiler and CMake 3.20 or newer. FFmpeg is
fetched automatically during the CMake configure step.

```bash
cmake -S . -B build-java -DCMAKE_BUILD_TYPE=Release -DENABLE_JAVA=ON -DBUILD_SHARED_LIBS=OFF
cmake --build build-java --config Release

mkdir -p java/build/native/linux-x86_64
cp build-java/bin/libavioflow_jni.so java/build/native/linux-x86_64/

gradle -p java \
  -Pavioflow.nativeClassifier=linux-x86_64 \
  -Pavioflow.nativeLibraryDir="$PWD/java/build/native/linux-x86_64" \
  test nativeJar
```

## License

MIT
