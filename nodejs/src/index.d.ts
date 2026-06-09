/**
 * avioflow - High-performance audio decoding library powered by FFmpeg
 * 
 * @module avioflow
 */

/** Audio stream metadata */
export interface Metadata {
    /** Duration in seconds. May be 0 for live streams. */
    duration: number;
    /** Sample rate in Hz (e.g., 44100, 48000). */
    sampleRate: number;
    /** Number of audio channels (1=mono, 2=stereo). */
    numChannels: number;
    /** Codec name (e.g., 'mp3float', 'aac', 'flac'). */
    codec: string;
    /** Total number of samples. May be updated at EOF for streams. */
    numSamples: number;
    /** Original sample format (e.g., 'fltp', 's16'). */
    sampleFormat: string;
    /** Bit rate in bits per second. */
    bitRate: number;
    /** Container format (e.g., 'mp3', 'mp4', 'ogg'). */
    container: string;
}

/** System audio device information */
export interface DeviceInfo {
    /** Unique device identifier used for opening. */
    name: string;
    /** Human-readable device name. */
    description: string;
    /** True if this is an output/loopback device. */
    isOutput: boolean;
}

/** Options for AudioDecoder constructor */
export interface AudioDecoderOptions {
    /** Target output sample rate in Hz. If not specified, uses source sample rate. */
    outputSampleRate?: number;
    /** Target number of output channels. If not specified, uses source channel count. */
    outputNumChannels?: number;
    /** Source sample rate for raw PCM streaming. Required for stream mode with raw PCM. */
    inputSampleRate?: number;
    /** Source channel count for raw PCM streaming. Required for stream mode with raw PCM. */
    inputChannels?: number;
    /** 
     * Source format for streaming. Options:
     * - "s16le": 16-bit signed little-endian PCM
     * - "f32le": 32-bit float little-endian PCM
     * - "aac": AAC audio (with ADTS headers)
     * - "mp3": MP3 frames
     * - "opus": Opus audio
     * Required for stream mode.
     */
    inputFormat?: string;
}

/** Result from the load() convenience function */
export interface LoadResult {
    /** Audio stream metadata */
    metadata: Metadata;
    /** Decoded audio samples. Array of Float32Array, one per channel. */
    samples: Float32Array[];
}

/** Result from the getWaveform() function */
export interface WaveformResult {
    /** Audio stream metadata */
    metadata: Metadata;
    /** Minimum values for each pixel column. Array of Float32Array, one per channel. */
    min: Float32Array[];
    /** Maximum values for each pixel column. Array of Float32Array, one per channel. */
    max: Float32Array[];
}

/**
 * High-performance audio decoder with file and streaming support.
 * 
 * Two operation modes:
 * - **File Mode**: Load from file path, URL, or device via loadFile()
 * - **Stream Mode**: Feed bytes via feed() and drain output with getFrame()/getSamples()
 * 
 * @example File Mode
 * ```typescript
 * const decoder = new AudioDecoder({ outputSampleRate: 44100 });
 * const meta = decoder.loadFile("audio.mp3");
 * const samples = decoder.getSamples();
 * ```
 * 
 * @example Stream Mode
 * ```typescript
 * const decoder = new AudioDecoder({ inputFormat: "s16le", inputSampleRate: 48000, inputChannels: 2 });
 * decoder.feed(rawBuffer);
 * let frame;
 * while ((frame = decoder.getFrame()) !== null) {
 *     processAudio(frame);
 * }
 * ```
 */
export class AudioDecoder {
    constructor(options?: AudioDecoderOptions);

    /**
     * Load audio from file path, URL, or device.
     * @param source File path, URL, or device identifier
     * @returns Metadata object with audio stream information
     * @throws Error if source cannot be opened or decoded
     */
    loadFile(source: string): Metadata;

    /**
     * Load complete encoded audio bytes from memory.
     * @param source Buffer with full audio bytes
     * @returns Metadata object with audio stream information
     * @throws Error if source cannot be opened
     */
    loadBuffer(source: Buffer): Metadata;

    /**
     * Get current audio stream metadata.
     * @returns Metadata object
     */
    getMetadata(): Metadata;

    /**
     * Feed raw encoded bytes for streaming decode.
     * First call starts streaming mode using constructor options.
     * @param buffer Raw encoded audio bytes
     * @throws Error if inputFormat was not specified in constructor
     */
    feed(buffer: Buffer): void;

    /**
     * Mark stream input complete and allow delayed frames to drain.
     */
    flush(): void;

    /**
     * Decode next available audio frame.
     * @returns Array of Float32Array (one per channel), or null if EOF/no data
     */
    getFrame(): Float32Array[] | null;

    /**
     * Drain currently available samples.
     * @returns Array of Float32Array (one per channel)
     */
    getSamples(): Float32Array[];

    /**
     * Check if end of stream has been reached.
     * @returns true if all audio data has been decoded
     */
    isFinished(): boolean;
}

/**
 * Set FFmpeg logging verbosity level.
 * @param level Log level: "quiet", "fatal", "error", "warning", "info", "debug", "trace"
 */
export function setLogLevel(level: string): void;

/**
 * List available system audio devices.
 * @returns Array of DeviceInfo objects
 */
export function listAudioDevices(): DeviceInfo[];

/**
 * Load an audio file and decode all samples in one call.
 * 
 * This is a convenience function that combines file loading and full
 * decoding in a single call. For large files or when you need frame-by-frame
 * control, use AudioDecoder directly.
 * 
 * @param path Path to audio file or URL
 * @param options Optional decoder options (outputSampleRate, outputNumChannels)
 * @returns Object containing metadata and samples (Float32Array per channel)
 * 
 * @example
 * ```typescript
 * const { metadata, samples } = avioflow.load("audio.mp3", { outputSampleRate: 16000 });
 * console.log(`Duration: ${metadata.duration}s, Channels: ${samples.length}`);
 * ```
 */
export function load(path: string, options?: Pick<AudioDecoderOptions, 'outputSampleRate' | 'outputNumChannels'>): LoadResult;

/**
 * Get waveform summary for visualization.
 * 
 * This function decodes the audio at a reduced sample rate (16000 Hz) and
 * calculates the min/max values for each pixel column to support zooming.
 * 
 * @param path Path to audio file or URL
 * @param samplesPerPixel How many samples to compress into one pixel (zoom level)
 * @returns Object containing metadata and min/max waveform data
 */
export function getWaveform(path: string, samplesPerPixel: number): WaveformResult;

declare const avioflow: {
    AudioDecoder: typeof AudioDecoder;
    setLogLevel: typeof setLogLevel;
    listAudioDevices: typeof listAudioDevices;
    load: typeof load;
    getWaveform: typeof getWaveform;
};

export default avioflow;
