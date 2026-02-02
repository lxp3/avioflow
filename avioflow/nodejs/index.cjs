/**
 * @file index.cjs
 * @brief CommonJS wrapper for avioflow (for compatibility with require())
 */

// Synchronous wrapper that loads the ESM module
let avioflowModule = null;

// Load the ESM module on first require
const loadPromise = import('./index.js').then(mod => {
    avioflowModule = mod.default || mod;
    return avioflowModule;
});

// Export a getter that returns the loaded module or throws if not ready
module.exports = new Proxy({}, {
    get(target, prop) {
        if (!avioflowModule) {
            throw new Error('avioflow module not loaded yet. Use async initialization: const avioflow = await require("avioflow/cjs")');
        }
        return avioflowModule[prop];
    }
});

// Also export the load promise for async initialization
module.exports.ready = loadPromise;
