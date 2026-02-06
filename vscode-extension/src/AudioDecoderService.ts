import * as path from 'path';
import * as fs from 'fs';

export interface AudioMetadata {
    duration: number;
    sampleRate: number;
    numChannels: number;
    codec: string;
    numSamples: number;
    sampleFormat: string;
    bitRate: number;
    container: string;
    fileSize: number;
}

/**
 * WASM-based audio decoder service with async loading support
 */
export class AudioDecoderService {
    private static instance: AudioDecoderService;
    private wasmModule: any = null;
    private extensionPath: string;
    private isInitializing = false;
    private initPromise: Promise<any> | null = null;

    private constructor(extensionPath: string) {
        this.extensionPath = extensionPath;
    }

    public static initialize(extensionPath: string): AudioDecoderService {
        if (!AudioDecoderService.instance) {
            AudioDecoderService.instance = new AudioDecoderService(extensionPath);
        }
        return AudioDecoderService.instance;
    }

    public static getInstance(): AudioDecoderService {
        if (!AudioDecoderService.instance) {
            throw new Error('AudioDecoderService must be initialized first');
        }
        return AudioDecoderService.instance;
    }

    private async loadWasm(): Promise<any> {
        if (this.wasmModule) {
            return this.wasmModule;
        }

        if (this.isInitializing && this.initPromise) {
            return this.initPromise;
        }

        this.isInitializing = true;

        try {
            this.initPromise = this.initializeWasm();
            this.wasmModule = await this.initPromise;
            return this.wasmModule;
        } finally {
            this.isInitializing = false;
            this.initPromise = null;
        }
    }

    private async initializeWasm(): Promise<any> {
        const wasmPath = path.join(this.extensionPath, 'out', 'wasm', 'avioflow.js');
        const wasmBinaryPath = path.join(this.extensionPath, 'out', 'wasm', 'avioflow.wasm');

        if (!fs.existsSync(wasmPath)) {
            throw new Error(`WASM module not found at ${wasmPath}`);
        }

        if (!fs.existsSync(wasmBinaryPath)) {
            throw new Error(`WASM binary not found at ${wasmBinaryPath}`);
        }

        console.log('[AudioDecoderService] Loading WASM module from', wasmPath);

        const createAvioflow = await import(wasmPath).then(m => m.default);

        if (typeof createAvioflow !== 'function') {
            throw new Error('WASM module does not export a default initialization function');
        }

        const avioflow = await createAvioflow();
        console.log('[AudioDecoderService] WASM module loaded successfully');

        return avioflow;
    }

    /**
     * Phase 1: Quick metadata loading (fast, ~10ms)
     */
    public async getMetadata(filePath: string): Promise<{ metadata: AudioMetadata; decoder: any; fileBuffer: Buffer }> {
        if (!fs.existsSync(filePath)) {
            throw new Error(`Audio file not found: ${filePath}`);
        }

        const stats = fs.statSync(filePath);
        const startTime = Date.now();

        const avioflow = await this.loadWasm();
        const fileBuffer = fs.readFileSync(filePath);
        const uint8Array = new Uint8Array(fileBuffer);

        console.log(`[AudioDecoderService] Opening ${path.basename(filePath)} (${fileBuffer.length} bytes)`);

        // Create decoder instance and open buffer
        const decoder = new avioflow.AudioDecoder();
        decoder.openBuffer(uint8Array);

        // Get metadata only (fast)
        const metaStart = Date.now();
        const rawMeta = decoder.getMetadata();
        const metaTime = Date.now() - metaStart;

        const metadata: AudioMetadata = {
            duration: rawMeta.duration,
            sampleRate: rawMeta.sampleRate,
            numChannels: rawMeta.numChannels,
            codec: rawMeta.codec,
            numSamples: rawMeta.numSamples,
            sampleFormat: rawMeta.sampleFormat,
            bitRate: rawMeta.bitRate,
            container: rawMeta.container,
            fileSize: stats.size
        };

        console.log(`[AudioDecoderService] Metadata loaded in ${metaTime}ms (total: ${Date.now() - startTime}ms)`);

        return { metadata, decoder, fileBuffer };
    }

    /**
     * Phase 2: Decode all samples (slow, async)
     */
    public async getSamples(decoder: any): Promise<{ samples: any[]; decodeTimeMs: number }> {
        const startTime = Date.now();

        console.log('[AudioDecoderService] Starting sample decoding...');

        // This is the slow operation
        const samples = decoder.getAllSamples();
        const decodeTimeMs = Date.now() - startTime;

        console.log(`[AudioDecoderService] Samples decoded in ${decodeTimeMs}ms`);

        // Convert to array
        const samplesArray: any[] = [];
        for (let i = 0; i < samples.length; i++) {
            samplesArray.push(samples[i]);
        }

        return { samples: samplesArray, decodeTimeMs };
    }

    /**
     * Legacy: Decode everything at once (for backwards compatibility)
     */
    public async decodeAudioFile(filePath: string): Promise<{ metadata: AudioMetadata; samples: any[]; loadTimeMs: number }> {
        const { metadata, decoder } = await this.getMetadata(filePath);
        const { samples, decodeTimeMs } = await this.getSamples(decoder);

        return {
            metadata,
            samples,
            loadTimeMs: decodeTimeMs
        };
    }

    public dispose() {
        this.wasmModule = null;
        console.log('[AudioDecoderService] Disposed');
    }
}
