import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const require = createRequire(import.meta.url);
const projectDir = join(dirname(fileURLToPath(import.meta.url)), '../..');

// Robustly load native module using node-gyp-build
// This supports local builds, prebuilds, and standard npm installs
export default require('node-gyp-build')(projectDir);
