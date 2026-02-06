import * as path from 'path';
import * as fs from 'fs';
import * as vscode from 'vscode';

export interface DecodeResult {
    metadata: any;
    samples?: any[];
    min?: any[];
    max?: any[];
    loadTimeMs?: number;
}

/**
 * WASM-based audio decoder service
 * Replaces the IPC-based AvioflowWorkerService with direct WASM decoding
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

    /**
     * Lazy-load the WASM module on first use
     */
    private async loadWasm(): Promise<any> {
        if (this.wasmModule) {
            return this.wasmModule;
        }

        // Prevent multiple concurrent initialization attempts
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

        // Dynamically import the WASM module
        // The avioflow.js file exports a default function that initializes the WASM module
        const createAvioflow = await import(wasmPath).then(m => m.default);

        if (typeof createAvioflow !== 'function') {
            throw new Error('WASM module does not export a default initialization function');
        }

        const avioflow = await createAvioflow();
        console.log('[AudioDecoderService] WASM module loaded successfully');

        return avioflow;
    }

    /**
     * Decode an audio file using WASM
     */
    public async decodeAudioFile(filePath: string, samplesPerPixel?: number): Promise<DecodeResult> {
        if (!fs.existsSync(filePath)) {
            throw new Error(`Audio file not found: ${filePath}`);
        }

        const startTime = Date.now();

        try {
            // Load WASM module if not already loaded
            const avioflow = await this.loadWasm();

            // Read the audio file
            const fileBuffer = fs.readFileSync(filePath);
            const uint8Array = new Uint8Array(fileBuffer);

            console.log(`[AudioDecoderService] Decoding ${path.basename(filePath)} (${fileBuffer.length} bytes)`);

            // Decode using WASM
            const decodeStart = Date.now();
            const result = avioflow.loadBuffer(uint8Array);
            const decodeTimeMs = Date.now() - decodeStart;

            if (!result || !result.metadata) {
                throw new Error('Failed to decode audio file');
            }

            // Calculate waveform summary if samplesPerPixel is specified
            let min: any[] | undefined;
            let max: any[] | undefined;
            let samples: any[] | undefined;

            if (result.samples && samplesPerPixel && samplesPerPixel > 1) {
                const summary = this.calculateWaveformSummary(result.samples, samplesPerPixel);
                min = summary.min;
                max = summary.max;
            } else if (result.samples) {
                samples = result.samples;
            }

            const totalTimeMs = Date.now() - startTime;

            console.log(`[AudioDecoderService] Decode complete: ${decodeTimeMs}ms (total: ${totalTimeMs}ms)`);

            return {
                metadata: result.metadata,
                samples,
                min,
                max,
                loadTimeMs: decodeTimeMs
            };
        } catch (error: any) {
            console.error('[AudioDecoderService] Decode error:', error);
            throw error;
        }
    }

    /**
     * Calculate waveform summary (min/max values per pixel)
     * This reduces the number of samples for efficient visualization
     */
    private calculateWaveformSummary(samples: any[], samplesPerPixel: number): { min: any[], max: any[] } {
        const min: any[] = [];
        const max: any[] = [];

        if (!samples || samples.length === 0) {
            return { min, max };
        }

        // WASM returns samples as [channel1_array, channel2_array]
        // Each channel is a Float32Array or Array
        const numChannels = samples.length;
        const numSamples = samples[0].length;

        // Calculate min/max for each pixel
        const numPixels = Math.ceil(numSamples / samplesPerPixel);

        for (let pixelIdx = 0; pixelIdx < numPixels; pixelIdx++) {
            const startSample = pixelIdx * samplesPerPixel;
            const endSample = Math.min(startSample + samplesPerPixel, numSamples);

            if (numChannels > 1) {
                // Stereo or multi-channel
                const minVals: number[] = [];
                const maxVals: number[] = [];

                for (let ch = 0; ch < numChannels; ch++) {
                    let chMin = Infinity;
                    let chMax = -Infinity;

                    for (let i = startSample; i < endSample; i++) {
                        const val = samples[ch][i];
                        if (val < chMin) chMin = val;
                        if (val > chMax) chMax = val;
                    }

                    minVals.push(chMin);
                    maxVals.push(chMax);
                }

                min.push(minVals);
                max.push(maxVals);
            } else {
                // Mono
                let chMin = Infinity;
                let chMax = -Infinity;

                for (let i = startSample; i < endSample; i++) {
                    const val = samples[0][i];
                    if (val < chMin) chMin = val;
                    if (val > chMax) chMax = val;
                }

                min.push(chMin);
                max.push(chMax);
            }
        }

        return { min, max };
    }

    public dispose() {
        this.wasmModule = null;
        console.log('[AudioDecoderService] Disposed');
    }
}
