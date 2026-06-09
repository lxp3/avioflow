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
    outputSampleRate?: number;
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
    
    /** Load audio from URL */
    loadFile(url: string): Metadata;
    
    /** Load complete audio bytes from buffer */
    loadBuffer(buffer: ArrayBuffer | Uint8Array): Metadata;
    
    /** Feed data for streaming decode */
    feed(data: Uint8Array): void;

    /** Mark stream input complete */
    flush(): void;
    
    /** Decode next frame */
    getFrame(): Float32Array[] | null;
    
    /** Drain currently available samples */
    getSamples(): Float32Array[];
    
    /** Get metadata */
    getMetadata(): Metadata;
    
    /** Check if finished */
    isFinished(): boolean;
}

/** Avioflow WASM module interface */
export interface AvioflowModule {
    AudioDecoder: typeof AudioDecoder;
    setLogLevel(level: string): void;
    load(url: string, options?: DecodeOptions): LoadResult;
    loadBuffer(buffer: ArrayBuffer | Uint8Array, options?: DecodeOptions): LoadResult;
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
