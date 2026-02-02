/**
 * @file index.js
 * @brief Native module loader for avioflow (Node.js & Electron compatible)
 * 
 * This module uses Node-API (formerly N-API) which provides ABI stability.
 * A single compiled .node file works across:
 * - Node.js 16, 18, 20, 22+ (all versions supporting Node-API 8)
 * - Electron 28, 30, 32, 34, 37, 38, 39+ (all versions with Node-API 8)
 * 
 * NO separate Electron-specific builds are required!
 */

import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { existsSync } from 'fs';

const require = createRequire(import.meta.url);
const __dirname = dirname(fileURLToPath(import.meta.url));
const projectDir = join(__dirname, '../..');

// Platform detection
const platform = process.platform === 'win32' ? 'win32' : process.platform === 'darwin' ? 'darwin' : 'linux';
const arch = process.arch;
const prebuildsDir = join(projectDir, 'prebuilds', `${platform}-${arch}`);

// Runtime info for debugging
const isElectron = !!(process.versions && process.versions.electron);
const runtimeInfo = isElectron 
  ? `Electron ${process.versions.electron} (Node ${process.versions.node})`
  : `Node.js ${process.versions.node}`;

let addon;

/**
 * Try to load a native module from a path
 * @param {string} modulePath - Path to the .node file
 * @param {string} description - Description for logging
 * @returns {boolean} - True if loaded successfully
 */
function tryLoad(modulePath, description) {
  if (!existsSync(modulePath)) {
    return false;
  }
  try {
    addon = require(modulePath);
    console.log(`[avioflow] Loaded ${description} from: ${modulePath}`);
    return true;
  } catch (e) {
    console.warn(`[avioflow] Failed to load ${description}: ${e.message}`);
    return false;
  }
}

// === Loading Strategy ===
// Node-API modules are ABI-stable, so we use ONE binary for all runtimes

// 1. Development builds (cmake-js output)
const devPaths = [
  join(projectDir, 'build/bin/Release/avioflow.node'),
  join(projectDir, 'build/bin/avioflow.node'),
  join(projectDir, 'build_static/bin/Release/avioflow.node'),
  join(projectDir, 'build_static/bin/avioflow.node'),
];

for (const p of devPaths) {
  if (tryLoad(p, 'development build')) break;
}

// 2. Prebuilt binaries (npm distribution)
// Node-API 8 binary works for BOTH Node.js and Electron!
if (!addon) {
  console.log(`[avioflow] Runtime: ${runtimeInfo}`);
  console.log(`[avioflow] Searching prebuilds in: ${prebuildsDir}`);
  
  // Single prebuild file for all runtimes (Node-API ABI stability)
  const prebuildPath = join(prebuildsDir, 'avioflow.napi.node');
  tryLoad(prebuildPath, 'prebuild (Node-API 8)');
}

// 3. Fallback to node-gyp-build (for compatibility with various build systems)
if (!addon) {
  try {
    console.log('[avioflow] Attempting node-gyp-build fallback...');
    addon = require('node-gyp-build')(projectDir);
    console.log('[avioflow] Loaded via node-gyp-build');
  } catch (e) {
    console.error(`[avioflow] Failed to load native module: ${e.message}`);
    console.error('[avioflow] Please ensure the native module is built or prebuilds are available.');
    throw new Error(`avioflow native module not found. Runtime: ${runtimeInfo}, Platform: ${platform}-${arch}`);
  }
}

// Export individual functions for easier access
export const setLogLevel = addon.setLogLevel;
export const listAudioDevices = addon.listAudioDevices;
export const load = addon.load;
export const getWaveform = addon.getWaveform;
export const AudioDecoder = addon.AudioDecoder;

export default addon;
