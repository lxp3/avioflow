# avioflow (Node.js)

Node.js bindings for [AvioFlow](https://github.com/lxp3/avioflow), a
high-performance audio library built on FFmpeg.

The package wraps the same C++ core as the Python, Java, Rust and WebAssembly
bindings, through a Node-API addon. FFmpeg is bundled in the platform package,
so there is nothing to install or locate at runtime.

## Install

```bash
npm install avioflow
```

The native binary is selected automatically from a platform-specific optional
dependency (`@lxp3/linux-x64`, `@lxp3/darwin-arm64`, and so on).

### Compatibility

| Runtime | Version | Support |
| ------- | ------- | ------- |
| Node.js | 16, 18, 20, 22+ | Native, via Node-API 8 |
| Electron | 28+ | Supported, no rebuild needed |
| Platforms | x64, arm64 | Linux, macOS, Windows |

Node-API 8 is ABI-stable, so one binary works across Node.js and Electron
versions without recompiling.

## Conventions

- **Samples are `Float32Array[]`**, one array per channel: `samples[channel]`
  holds that channel's data, and every channel has the same length.
- **Failures throw.** A source that cannot be opened or decoded throws an
  `Error` carrying the message from the native layer.
- **Unset options preserve the source format.** Omitting a rate or channel
  count, or passing `-1`, keeps whatever the input has.

## Quick start

```javascript
import avioflow from 'avioflow';

const { metadata, samples } = avioflow.load('audio.mp3', {
    outputSampleRate: 16000,
    outputNumChannels: 1,
});

console.log(`${metadata.duration}s at ${metadata.sampleRate} Hz`);
console.log(`${samples.length} channels x ${samples[0].length} samples`);
```

CommonJS works too: `const avioflow = require('avioflow');`

## Module functions

| Function | Returns | Description |
| -------- | ------- | ----------- |
| `load(path, options?)` | `{metadata, samples}` | Open and decode everything in one call. |
| `loadAsync(path, options?)` | `Promise<{metadata, samples}>` | Same, off the event loop. |
| `save(path, samples, options?)` | `void` | Write samples to a file. |
| `getWaveform(path, samplesPerPixel)` | `{metadata, min, max}` | Min/max summary for visualization. |
| `resample(samples, inputSampleRate, outputSampleRate, outputNumChannels?)` | `Float32Array[]` | Resample a complete buffer. |
| `listAudioDevices()` | `DeviceInfo[]` | Enumerate audio devices. |
| `setLogLevel(level)` | `void` | FFmpeg verbosity. |
| `getSupportedDecoders()` | `string[]` | Available decoders, e.g. `"mp3"`. |
| `getSupportedEncoders()` | `string[]` | Available encoders, e.g. `"pcm_s16le"`. |
| `getSupportedInputFormats()` | `string[]` | Available demuxers. |
| `getSupportedOutputFormats()` | `string[]` | Available muxers. |

`load` blocks the event loop for the duration of the decode. In a server, prefer
`loadAsync`.

## Decoding

### `AudioDecoder`

```javascript
const decoder = new avioflow.AudioDecoder({
    outputSampleRate: 16000,   // resample; omit to keep the source rate
    outputNumChannels: 1,      // remix; omit to keep the source channels
    inputFormat: 's16le',      // required for streaming
    inputSampleRate: 48000,    // required for raw PCM streaming
    inputChannels: 2,          // required for raw PCM streaming
});
```

| Method | Returns | Description |
| ------ | ------- | ----------- |
| `loadFile(source)` | `Metadata` | Open a path, URL or device. |
| `loadBuffer(buffer)` | `Metadata` | Open complete file bytes in memory. |
| `feed(buffer)` | `void` | Push encoded bytes for streaming decode. |
| `flush()` | `void` | Mark streaming input complete. |
| `getSamples(startSeconds?, stopSeconds?)` | `Float32Array[]` | Decode `[start, stop)` in seconds. |
| `getFrame()` | `Float32Array[] \| null` | Decode one frame; `null` at end of stream. |
| `isFinished()` | `boolean` | Whether the stream is exhausted. |
| `getMetadata()` | `Metadata` | Current stream metadata. |

### Offline decoding

```javascript
const decoder = new avioflow.AudioDecoder({ outputSampleRate: 44100 });
const metadata = decoder.loadFile('audio.wav');
const samples = decoder.getSamples();
```

`loadFile` reports the *source* stream. The resampler is not configured until
the first frame is decoded, so when `outputSampleRate` is set the new rate
appears in `getMetadata()` after decoding rather than in what `loadFile`
returns.

### Time-range decoding

```javascript
const decoder = new avioflow.AudioDecoder();
decoder.loadFile('audio.mp3');

// Seconds 10.3 through 20.3, exclusive of the end
const window = decoder.getSamples(10.3, 20.3);

// From 30s to the end
const tail = decoder.getSamples(30.0);
```

Each call seeks independently, so one decoder can serve many ranges. Range
decoding requires offline mode; in stream mode `getSamples()` drains whatever is
currently buffered and the range arguments do not apply.

### Frame-by-frame

```javascript
const decoder = new avioflow.AudioDecoder();
decoder.loadFile('audio.mp3');

let frame;
let total = 0;
while ((frame = decoder.getFrame()) !== null) {
    total += frame[0].length;
}
```

### Streaming decode

```javascript
const decoder = new avioflow.AudioDecoder({
    inputFormat: 's16le',
    inputSampleRate: 48000,
    inputChannels: 2,
});

socket.on('data', (chunk) => {
    decoder.feed(chunk);
    const samples = decoder.getSamples();
    if (samples.length > 0) processAudio(samples);
});

socket.on('end', () => {
    decoder.flush();                       // then drain the remainder
    const remaining = decoder.getSamples();
    if (remaining.length > 0) processAudio(remaining);
});
```

`flush()` discards nothing. It marks input complete so buffered bytes and
codec-delayed frames can still be drained. For raw PCM input, also set
`inputSampleRate` and `inputChannels`.

## Encoding

```javascript
avioflow.save('out.wav', samples, {
    containerFormat: 'wav',
    codecName: 'pcm_s16le',
    sampleRate: 16000,
});
```

### `AudioWriteOptions`

| Field | Common values |
| ----- | ------------- |
| `codecName` | `"pcm_s16le"`, `"flac"`, `"aac"`, `"libmp3lame"`, `"libopus"` |
| `containerFormat` | `"wav"`, `"flac"`, `"mp4"`, `"ogg"`, `"adts"` |
| `sampleFormat` | `"s16"`, `"s32"`, `"flt"`, `"fltp"` |
| `sampleRate` | 8000, 16000, 44100, 48000 |
| `numChannels` | 1, 2 |
| `bitRate` | 128000, 192000, 320000 |
| `overwrite` | Defaults to `true` |

Unset fields are inferred from the container and the input samples.

## Resampling

Two entry points: `resample` for a buffer held in full, `AudioResampler` for
audio arriving in chunks. Both work on any `Float32Array[]`, not only avioflow
output.

To resample *while decoding*, pass `outputSampleRate` to `AudioDecoder` instead;
that avoids a second pass over the samples.

### One-shot

```javascript
const downsampled = avioflow.resample(samples, 44100, 16000);
const mono = avioflow.resample(samples, 44100, 16000, 1);
```

### Chunked

```javascript
const resampler = new avioflow.AudioResampler({
    inputSampleRate: 44100,
    outputSampleRate: 16000,
});

const parts = [];
for (const chunk of chunks) parts.push(resampler.process(chunk));
parts.push(resampler.flush());     // else the tail is lost
```

`flush()` is not optional. The resampler holds back the last few milliseconds to
keep filter continuity, and skipping the flush discards them. Filter state
carries across `process` calls, so chunked output matches a one-shot conversion
sample for sample — calling `resample` per chunk instead would introduce a
discontinuity at every boundary.

`outputNumChannels()` returns 0 until the first `process` call reveals the input
channel count. The channel count must not change between calls.

## Waveform summaries

For drawing a waveform, decoding every sample is wasteful. `getWaveform`
decodes at a reduced rate and returns per-column minima and maxima:

```javascript
const { metadata, min, max } = avioflow.getWaveform('audio.mp3', 512);

// min[channel][column] and max[channel][column] bound each pixel column
for (let i = 0; i < min[0].length; i++) {
    drawColumn(i, min[0][i], max[0][i]);
}
```

`samplesPerPixel` is the zoom level: how many source samples collapse into one
column.

## Metadata

| Field | Type | Description |
| ----- | ---- | ----------- |
| `duration` | `number` | Seconds; 0 for live streams. |
| `numSamples` | `number` | Samples per channel; updated at EOF for streams. |
| `sampleRate` | `number` | Hz. |
| `numChannels` | `number` | 1 for mono, 2 for stereo. |
| `sampleFormat` | `string` | Source format, e.g. `"fltp"`. |
| `codec` | `string` | Decoder name, e.g. `"mp3float"`. |
| `bitRate` | `number` | Bits per second. |
| `container` | `string` | Container format, e.g. `"mp3"`. |

Note `codec` is the FFmpeg *decoder* name, so an MP3 file reports
`"mp3float"` while `container` reports `"mp3"`.

## Devices and diagnostics

```javascript
avioflow.listAudioDevices().forEach((dev) => {
    console.log(`${dev.isOutput ? 'Output' : 'Input'}: ${dev.name} (${dev.description})`);
});

// Open a device by passing its name as the source
const decoder = new avioflow.AudioDecoder();
decoder.loadFile('audio=Microphone');

avioflow.setLogLevel('debug');   // quiet, error, warning, info, debug, trace
```

## TypeScript

Type definitions ship with the package; no `@types` install is needed.

```typescript
import avioflow, { Metadata, AudioDecoderOptions } from 'avioflow';
```

## Build from source

```bash
cd nodejs && npm install && npx cmake-js compile
```

Building compiles the C++ core, so the host needs a C++17 compiler and CMake
3.20 or newer. FFmpeg is fetched automatically during the CMake configure step.

Run the tests against a checkout:

```bash
node nodejs/tests/test-offline-load.js public/wavs/TownTheme.mp3
```

## License

MIT
