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

/** Avioflow WASM module interface */
export interface AvioflowModule {
    AudioDecoder: typeof AudioDecoder;
    setLogLevel(level: string): void;
    /** Decodes a path in the Emscripten virtual filesystem, not a network URL. */
    load(path: string, options?: DecodeOptions): LoadResult;
    loadBuffer(buffer: ArrayBuffer | Uint8Array, options?: DecodeOptions): LoadResult;
    /** Encodes samples to a path in the Emscripten virtual filesystem. */
    save(path: string, channels: Float32Array[], options?: WriteOptions): void;
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
