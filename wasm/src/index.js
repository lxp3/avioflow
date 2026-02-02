/**
 * @file index.js
 * @brief JavaScript wrapper for avioflow WebAssembly module
 *
 * Usage:
 *   import { createAvioflow } from 'avioflow-wasm';
 *   const avioflow = await createAvioflow();
 *   const { metadata, samples } = avioflow.loadBuffer(arrayBuffer);
 */

// Re-export the WASM module factory
export { default as createAvioflow } from './avioflow.js';

/**
 * Helper to create and initialize the module
 * @returns {Promise<Object>} Initialized avioflow module
 */
export async function initAvioflow() {
    const { default: createAvioflow } = await import('./avioflow.js');
    return await createAvioflow();
}

/**
 * Convenience function to decode audio buffer
 * @param {ArrayBuffer|Uint8Array} buffer - Audio data
 * @param {Object} options - Decode options
 * @returns {Promise<{metadata: Object, samples: Float32Array[]}>}
 */
export async function decodeAudioBuffer(buffer, options = {}) {
    const avioflow = await initAvioflow();
    return avioflow.loadBuffer(buffer, options);
}

/**
 * Convenience function to decode audio from URL
 * @param {string} url - Audio URL
 * @param {Object} options - Decode options
 * @returns {Promise<{metadata: Object, samples: Float32Array[]}>}
 */
export async function decodeAudioUrl(url, options = {}) {
    const avioflow = await initAvioflow();
    return avioflow.load(url, options);
}
