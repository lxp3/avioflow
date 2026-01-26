import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const require = createRequire(import.meta.url);
const projectRoot = join(dirname(fileURLToPath(import.meta.url)), '../..');

// Load native module from build directory
const bindings = require(join(projectRoot, 'build/bin/Release/avioflow.node'));

export default bindings;
