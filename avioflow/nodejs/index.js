import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { existsSync } from 'fs';

const require = createRequire(import.meta.url);
const __dirname = dirname(fileURLToPath(import.meta.url));
const projectDir = join(__dirname, '../..');

// Try to load native module:
// 1. First try cmake-js local build (for development)
// 2. Then try node-gyp-build prebuilds (for npm distribution)
let addon;

// Check cmake-js build locations
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

// Fall back to prebuilds via node-gyp-build
if (!addon) {
  addon = require('node-gyp-build')(projectDir);
}

export default addon;
