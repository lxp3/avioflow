import * as path from 'path';
import * as fs from 'fs';
import * as vscode from 'vscode';

export interface DecodeResult {
    metadata: any;
    samples?: any[];
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
    public async decodeAudioFile(filePath: string): Promise<DecodeResult> {
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
            // Pass empty object as options to match C++ signature: loadBuffer(buffer, options)
            const result = avioflow.loadBuffer(uint8Array, {});
            const decodeTimeMs = Date.now() - decodeStart;

            if (!result || !result.metadata) {
                throw new Error('Failed to decode audio file');
            }

            const totalTimeMs = Date.now() - startTime;

            console.log(`[AudioDecoderService] Decode complete: ${decodeTimeMs}ms (total: ${totalTimeMs}ms)`);

            return {
                metadata: result.metadata,
                samples: result.samples,
                loadTimeMs: decodeTimeMs
            };
        } catch (error: any) {
            console.error('[AudioDecoderService] Decode error:', error);
            throw error;
        }
    }

    public dispose() {
        this.wasmModule = null;
        console.log('[AudioDecoderService] Disposed');
    }
}
