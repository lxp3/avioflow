/**
 * @file avioflowWorker.mjs
 * @brief Standalone worker for avioflow decoding (Pure ES Module)
 * 
 * This file is .mjs to ensure Node.js treats it as a native ES Module,
 * allowing proper dynamic import of the ESM-based avioflow package.
 */

import { statSync } from 'fs';

let avioflow;

async function main() {
    // Load avioflow module (native ESM import)
    try {
        const mod = await import('avioflow');
        avioflow = mod.default || mod;
        console.log('[AvioflowWorker] Avioflow module loaded successfully');
    } catch (error) {
        console.error('[AvioflowWorker] Failed to load avioflow:', error);
        process.exit(1);
    }

    process.on('message', async (msg) => {
        if (msg.type === 'load') {
            const { filePath, samplesPerPixel } = msg;
            try {
                const start = Date.now();
                
                // Always load full samples for playback
                const result = avioflow.load(filePath);
                
                // Calculate waveform summary in worker to avoid UI lag
                let min, max;
                if (samplesPerPixel && result.samples && result.samples.length > 0) {
                    const numChannels = result.samples.length;
                    const totalSamples = result.samples[0].length;
                    
                    // Use metadata.numSamples if available, otherwise use actual length
                    const actualTotalSamples = (result.metadata && result.metadata.numSamples > 0) 
                        ? result.metadata.numSamples 
                        : totalSamples;
                    
                    console.log(`[AvioflowWorker] Calculating waveform: actualTotalSamples=${actualTotalSamples}, samplesPerPixel=${samplesPerPixel}`);
                    
                    const numPixels = Math.ceil(actualTotalSamples / samplesPerPixel);
                    
                    min = new Array(numChannels);
                    max = new Array(numChannels);
                    
                    for (let c = 0; c < numChannels; c++) {
                        const channelData = result.samples[c];
                        const minData = new Float32Array(numPixels);
                        const maxData = new Float32Array(numPixels);
                        
                        for (let p = 0; p < numPixels; p++) {
                            let minVal = 0;
                            let maxVal = 0;
                            const startIdx = p * samplesPerPixel;
                            const endIdx = Math.min(startIdx + samplesPerPixel, totalSamples);
                            
                            if (startIdx < totalSamples) {
                                minVal = channelData[startIdx];
                                maxVal = channelData[startIdx];
                                for (let i = startIdx + 1; i < endIdx; i++) {
                                    const val = channelData[i];
                                    if (val < minVal) minVal = val;
                                    if (val > maxVal) maxVal = val;
                                }
                            }
                            minData[p] = minVal;
                            maxData[p] = maxVal;
                        }
                        min[c] = minData;
                        max[c] = maxData;
                    }
                }

                const loadTimeMs = Date.now() - start;
                const stats = statSync(filePath);
                
                // Log to stderr so it appears in VS Code's output
                console.error(`[AvioflowWorker] Native load/decode complete in ${loadTimeMs}ms`);

                // Message back to parent
                const response = {
                    type: 'done',
                    metadata: {
                        ...result.metadata,
                        fileSize: stats.size
                    },
                    min: min,
                    max: max,
                    samples: result.samples,
                    loadTimeMs: loadTimeMs  // Include load time in response
                };

                // Use advanced serialization to transfer ArrayBuffers without copying
                // This significantly reduces IPC overhead for large audio data
                if (process.send) {
                    // Extract ArrayBuffers for zero-copy transfer
                    const transferList = [];
                    if (result.min) result.min.forEach(ch => transferList.push(ch.buffer));
                    if (result.max) result.max.forEach(ch => transferList.push(ch.buffer));
                    if (result.samples) result.samples.forEach(ch => transferList.push(ch.buffer));
                    
                    process.send(response, undefined, undefined, (err) => {
                        if (err) console.error('[AvioflowWorker] IPC Send Error:', err);
                    });
                }
            } catch (error) {
                console.error(`[AvioflowWorker] Error loading ${filePath}:`, error.message);
                process.send({
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

main().catch(error => {
    console.error('[AvioflowWorker] Fatal error:', error);
    process.exit(1);
});
