import * as child_process from 'child_process';
import * as path from 'path';
import * as fs from 'fs';
import * as vscode from 'vscode';

export interface WorkerResponse {
    type: 'done' | 'error' | 'ready';
    metadata?: any;
    samples?: any[];
    message?: string;
    stack?: string;
}

export class AvioflowWorkerService {
    private static instance: AvioflowWorkerService;
    private worker: child_process.ChildProcess | null = null;
    private pendingRequests = new Map<string, { resolve: (val: any) => void, reject: (err: any) => void }>();
    private isReady = false;
    private readyCallbacks: (() => void)[] = [];

    private constructor(private readonly extensionPath: string) { }

    public static initialize(extensionPath: string): AvioflowWorkerService {
        if (!AvioflowWorkerService.instance) {
            AvioflowWorkerService.instance = new AvioflowWorkerService(extensionPath);
            AvioflowWorkerService.instance.startWorker();
        }
        return AvioflowWorkerService.instance;
    }

    public static getInstance(): AvioflowWorkerService {
        if (!AvioflowWorkerService.instance) {
            throw new Error('AvioflowWorkerService must be initialized first');
        }
        return AvioflowWorkerService.instance;
    }

    private startWorker() {
        if (this.worker) {
            this.worker.kill();
        }

        const workerPath = path.join(this.extensionPath, 'out', 'src', 'avioflowWorker.js');
        if (!fs.existsSync(workerPath)) {
            console.error(`[AvioflowService] Worker script not found at ${workerPath}`);
            return;
        }


        this.worker = child_process.spawn('node', [workerPath], {
            stdio: ['ignore', 'pipe', 'pipe', 'ipc'],
            env: { ...process.env },
            windowsHide: true,
            serialization: 'advanced'
        });

        this.worker.stdout?.on('data', (data) => {
            console.log(`[AvioflowWorker Output]: ${data.toString().trim()}`);
        });

        this.worker.stderr?.on('data', (data) => {
            console.error(`[AvioflowWorker Error]: ${data.toString().trim()}`);
        });

        this.worker.on('message', (msg: WorkerResponse) => {
            if (msg.type === 'ready') {
                this.isReady = true;
                this.readyCallbacks.forEach(cb => cb());
                this.readyCallbacks = [];
            } else if (msg.type === 'done' || msg.type === 'error') {
                // We use a simple protocol where we assume requests are processed one by one 
                // for simplicity, or we can add a requestId. 
                // Since this is for a previewer, one by one is likely fine for now.
                // But let's add a basic requestId support in the worker too if needed.
                // For now, let's assume single-tasking for the worker.
                this.handleResponse(msg);
            }
        });

        this.worker.on('exit', (code) => {
            console.warn(`[AvioflowService] Worker exited with code ${code}. Restarting...`);
            this.isReady = false;
            this.worker = null;
            // Clear pending requests on crash
            this.pendingRequests.forEach(req => req.reject(new Error('Worker process crashed')));
            this.pendingRequests.clear();

            // Auto restart after a short delay
            setTimeout(() => this.startWorker(), 1000);
        });

        this.worker.on('error', (err) => {
            console.error('[AvioflowService] Worker process error:', err);
        });
    }

    private handleResponse(msg: WorkerResponse) {
        // Find the first pending request.
        const firstKey = this.pendingRequests.keys().next().value;
        if (firstKey) {
            const { resolve, reject } = this.pendingRequests.get(firstKey)!;
            this.pendingRequests.delete(firstKey);

            if (msg.type === 'done') {
                resolve({ metadata: msg.metadata, samples: msg.samples });
            } else {
                reject(new Error(msg.message || 'Unknown worker error'));
            }
        }
    }

    public async load(filePath: string): Promise<{ metadata: any, samples: any[] }> {
        if (!this.worker) {
            console.log('[AvioflowService] First request received, starting worker lazy...');
            this.startWorker();
        }

        if (!this.isReady) {
            await new Promise<void>(resolve => this.readyCallbacks.push(resolve));
        }

        return new Promise((resolve, reject) => {
            // If there's already a request for this file, just add to the queue?
            // Actually, let's just use the filePath as the key for simplicity.
            // If multiple editors open the same file, it's fine.
            this.pendingRequests.set(filePath, { resolve, reject });
            this.worker!.send({ type: 'load', filePath });
        });
    }

    public dispose() {
        if (this.worker) {
            this.worker.kill();
            this.worker = null;
        }
    }
}
