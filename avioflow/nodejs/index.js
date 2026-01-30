import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { existsSync } from 'fs';

const require = createRequire(import.meta.url);
const __dirname = dirname(fileURLToPath(import.meta.url));
const projectDir = join(__dirname, '../..');

// Detect if running in Electron (VS Code extension environment)
const isElectron = !!(process.versions && process.versions.electron);
const platform = process.platform === 'win32' ? 'win32' : process.platform === 'darwin' ? 'darwin' : 'linux';
const arch = process.arch;
const prebuildsDir = join(projectDir, 'prebuilds', `${platform}-${arch}`);

// Try to load native module
let addon;

// Check cmake-js build locations (for development)
const cmakeBuildPaths = [
  join(projectDir, 'build/bin/Release/avioflow.node'),
  join(projectDir, 'build/bin/avioflow.node'),
  join(projectDir, 'build_static/bin/Release/avioflow.node'),
  join(projectDir, 'build_static/bin/avioflow.node'),
];

for (const p of cmakeBuildPaths) {
  if (existsSync(p)) {
    addon = require(p);
    break;
  }
}

// Try prebuilds (for npm distribution)
if (!addon) {
  console.log(`[avioflow] Searching for prebuilds in: ${prebuildsDir}`);
  // For Electron, prefer electron-specific prebuild
  if (isElectron) {
    const electronPrebuild = join(prebuildsDir, 'electron.napi.node');
    console.log(`[avioflow] Detected Electron ${process.versions.electron}. Checking: ${electronPrebuild}`);
    if (existsSync(electronPrebuild)) {
      try {
        console.log(`[avioflow] Attempting to require electron-prebuild...`);
        addon = require(electronPrebuild);
        console.log('[avioflow] Successfully loaded Electron prebuild');
      } catch (e) {
        console.warn(`[avioflow] Failed to load Electron prebuild: ${e.message}`);
      }
    } else {
      console.warn('[avioflow] Electron prebuild file not found');
    }
  }

  // Fall back to Node.js prebuild
  if (!addon) {
    const nodePrebuild = join(prebuildsDir, 'avioflow.napi.node');
    console.log(`[avioflow] Checking Node.js prebuild: ${nodePrebuild}`);
    if (existsSync(nodePrebuild)) {
      try {
        addon = require(nodePrebuild);
        console.log('[avioflow] Successfully loaded Node.js prebuild');
      } catch (e) {
        console.warn(`[avioflow] Failed to load Node.js prebuild: ${e.message}`);
      }
    }
  }

  // Last resort: use node-gyp-build
  if (!addon) {
    addon = require('node-gyp-build')(projectDir);
  }
}

export default addon;
