/**
 * @file index.js
 * @brief Native module loader for avioflow (Node.js & Electron compatible)
 * 
 * This module uses Node-API (formerly N-API) which provides ABI stability.
 * A single compiled .node file works across:
 * - Node.js 16, 18, 20, 22+ (all versions supporting Node-API 8)
 * - Electron 28, 30, 32, 34, 37, 38, 39+ (all versions with Node-API 8)
 * 
 * Native bindings are loaded from platform-specific optional dependencies:
 * - @avioflow/win32-x64 (Windows x64)
 * - @avioflow/linux-x64 (Linux x64)
 */

import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { existsSync } from 'fs';

const require = createRequire(import.meta.url);
const __dirname = dirname(fileURLToPath(import.meta.url));
const nodejsDir = join(__dirname, '..');  // nodejs/ directory

// Platform detection
const platform = process.platform;
const arch = process.arch;
const platformKey = `${platform}-${arch}`;

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

/**
 * Try to require a package by name
 * @param {string} packageName - NPM package name
 * @param {string} description - Description for logging
 * @returns {boolean} - True if loaded successfully
 */
function tryRequire(packageName, description) {
  try {
    addon = require(packageName);
    console.log(`[avioflow] Loaded ${description} from package: ${packageName}`);
    return true;
  } catch (e) {
    // Package not installed (expected for non-matching platforms)
    return false;
  }
}

// === Loading Strategy ===
// Priority: 1) Dev builds -> 2) Platform packages -> 3) Legacy prebuilds

// 1. Development builds (cmake-js output) - for local development
const devPaths = [
  process.env.AVIOFLOW_BINDINGS_PATH, // Allow manual override via env var
  join(nodejsDir, 'build-nodejs/Release/avioflow.node'), // cmake-js custom out dir
  join(nodejsDir, 'build/bin/Release/avioflow.node'),
  join(nodejsDir, 'build/bin/avioflow.node'),
  join(nodejsDir, 'build_static/bin/Release/avioflow.node'),
  join(nodejsDir, 'build_static/bin/avioflow.node'),
];

for (const p of devPaths) {
  if (tryLoad(p, 'development build')) break;
}

// 2. Platform-specific npm packages (recommended for production)
// npm automatically installs only the package for current platform via optionalDependencies
if (!addon) {
  console.log(`[avioflow] Runtime: ${runtimeInfo}, Platform: ${platformKey}`);
  
  // Map platform to package name
  const platformPackages = {
    'win32-x64': '@avioflow/win32-x64',
    'linux-x64': '@avioflow/linux-x64',
    'darwin-x64': '@avioflow/darwin-x64',
    'darwin-arm64': '@avioflow/darwin-arm64',
  };
  
  const packageName = platformPackages[platformKey];
  if (packageName) {
    tryRequire(packageName, `platform package (${platformKey})`);
  }
}

// 3. Legacy: prebuilds directory (for backward compatibility)
if (!addon) {
  const prebuildsDir = join(nodejsDir, 'prebuilds', platformKey);
  const prebuildPath = join(prebuildsDir, 'avioflow.napi.node');
  tryLoad(prebuildPath, 'legacy prebuild');
}

// 4. Final check
if (!addon) {
  const supportedPlatforms = ['win32-x64', 'linux-x64'];
  const errorMsg = supportedPlatforms.includes(platformKey)
    ? `Native module not found. Please reinstall: npm install avioflow`
    : `Platform ${platformKey} is not supported. Supported: ${supportedPlatforms.join(', ')}`;
  
  console.error(`[avioflow] ${errorMsg}`);
  console.error(`[avioflow] Runtime: ${runtimeInfo}`);
  throw new Error(`avioflow: ${errorMsg}`);
}

// Export individual functions for easier access
export const setLogLevel = addon.setLogLevel;
export const listAudioDevices = addon.listAudioDevices;
export const load = addon.load;
export const getWaveform = addon.getWaveform;
export const AudioDecoder = addon.AudioDecoder;

export default addon;
