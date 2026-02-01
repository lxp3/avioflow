/**
 * @file avioflowWorker.ts
 * @brief Standalone worker for avioflow decoding to avoid Electron ABI issues.
 */

import { parentPort, workerData } from 'worker_threads';
import * as path from 'path';

// Using child_process.fork is often more reliable than worker_threads for native modules
// if there are thread-safety issues, but since we just need a clean Node.js environment,
// fork() is better as it gives us a separate process.
// However, I'll implement this to be run via 'node' directly.

function main() {
    process.on('message', (msg: any) => {
        if (msg.type === 'load') {
            const { filePath } = msg;
            try {
                // Dynamic require to load avioflow in this clean Node.js process
                const mod = require('avioflow');
                const avioflow = mod.default || mod;
                const start = Date.now();
                const result = avioflow.load(filePath);
                const duration = Date.now() - start;
                const stats = require('fs').statSync(filePath);
                console.log(`[AvioflowWorker] Native load/decode complete in ${duration}ms`);

                // Message back to parent
                process.send!({
                    type: 'done',
                    metadata: {
                        ...result.metadata,
                        fileSize: stats.size
                    },
                    samples: result.samples // Direct transfer of Float32Array
                });
            } catch (error: any) {
                process.send!({
                    type: 'error',
                    message: error.message,
                    stack: error.stack
                });
            }
        }
    });

    // Notify parent we are ready
    if (process.send) {
        process.send({ type: 'ready' });
    }
}

main();
