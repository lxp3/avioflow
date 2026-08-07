/**
 * avioflow WebAssembly Type Definitions
 */

/** Audio metadata */
export interface Metadata {
    duration: number;
    sampleRate: number;
    numChannels: number;
    codec: string;
    numSamples: number;
    sampleFormat: string;
    bitRate: number;
    container: string;
}

/** Decode options */
export interface DecodeOptions {
    /** Target output sample rate; -1 or omitted preserves the source rate. */
    outputSampleRate?: number;
    /** Target output channels; -1 or omitted preserves the source channel count. */
    outputNumChannels?: number;
}

/** Streaming decoder options */
export interface StreamOptions extends DecodeOptions {
    inputSampleRate?: number;
    inputChannels?: number;
    inputFormat?: string;
}

/** Load result */
export interface LoadResult {
    metadata: Metadata;
    samples: Float32Array[];
}

/** AudioDecoder class */
export declare class AudioDecoder {
    constructor(options?: StreamOptions);
    
    /** Load audio from a path in the Emscripten virtual filesystem. */
    loadFile(path: string): Metadata;
    
    /** Load complete audio bytes from buffer */
    loadBuffer(buffer: ArrayBuffer | Uint8Array): Metadata;
    
    /** Feed data for streaming decode */
    feed(data: Uint8Array): void;

    /** Mark stream input complete */
    flush(): void;
    
    /** Decode next frame */
    getFrame(): Float32Array[] | null;
    
    /**
     * Decode samples in the half-open range [startSeconds, stopSeconds).
     * Offline mode only. Pass -1 for stopSeconds to decode to the end.
     */
    getSamples(startSeconds?: number, stopSeconds?: number): Float32Array[];
    
    /** Get metadata */
    getMetadata(): Metadata;
    
    /** Check if finished */
    isFinished(): boolean;
}

/** Encode options for save() */
export interface WriteOptions {
    codecName?: string;
    containerFormat?: string;
    sampleRate?: number;
    numChannels?: number;
    bitRate?: number;
    sampleFormat?: string;
    overwrite?: boolean;
}

/** Options for the AudioResampler constructor */
export interface ResampleOptions {
    /** Source sample rate in Hz. Required, must be greater than zero. */
    inputSampleRate: number;
    /** Target sample rate in Hz. Required, must be greater than zero. */
    outputSampleRate: number;
    /** Target channel count. Defaults to the input channel count. */
    outputNumChannels?: number;
}

/**
 * Stateful resampler for audio that arrives in chunks.
 *
 * Filter state is preserved across `process()` calls, so consecutive chunks join
 * without discontinuities. `flush()` is not optional: the resampler holds back
 * the last few milliseconds of audio, and skipping the flush discards them.
 */
export declare class AudioResampler {
    constructor(options: ResampleOptions);
    /** Resample one chunk; may emit fewer samples than the ratio suggests. */
    process(channels: Float32Array[]): Float32Array[];
    /** Drain buffered samples. Call once after the final process(). */
    flush(): Float32Array[];
    /** The output sample rate the resampler was configured with. */
    outputSampleRate(): number;
    /** Output channel count. Zero until the first process() call. */
    outputNumChannels(): number;
    /** Release the underlying WASM object. */
    delete(): void;
}

/** Avioflow WASM module interface */
export interface AvioflowModule {
    AudioDecoder: typeof AudioDecoder;
    AudioResampler: typeof AudioResampler;
    setLogLevel(level: string): void;
    /** Decodes a path in the Emscripten virtual filesystem, not a network URL. */
    load(path: string, options?: DecodeOptions): LoadResult;
    loadBuffer(buffer: ArrayBuffer | Uint8Array, options?: DecodeOptions): LoadResult;
    /** Encodes samples to a path in the Emscripten virtual filesystem. */
    save(path: string, channels: Float32Array[], options?: WriteOptions): void;
    /**
     * Resample a complete buffer in one call.
     * @param outputNumChannels Target channel count; pass -1 to keep the input count.
     */
    resample(
        channels: Float32Array[],
        inputSampleRate: number,
        outputSampleRate: number,
        outputNumChannels: number
    ): Float32Array[];
    getSupportedDecoders(): string[];
    getSupportedEncoders(): string[];
    getSupportedInputFormats(): string[];
    getSupportedOutputFormats(): string[];
}

/** Create and initialize the WASM module */
export declare function createAvioflow(): Promise<AvioflowModule>;

/** Initialize avioflow (alias) */
export declare function initAvioflow(): Promise<AvioflowModule>;

/** Decode audio buffer */
export declare function decodeAudioBuffer(
    buffer: ArrayBuffer | Uint8Array,
    options?: DecodeOptions
): Promise<LoadResult>;

/** Decode audio from URL */
export declare function decodeAudioUrl(
    url: string,
    options?: DecodeOptions
): Promise<LoadResult>;

export default createAvioflow;
