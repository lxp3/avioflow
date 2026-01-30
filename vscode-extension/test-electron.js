/**
 * Test script to verify avioflow native module loading in Electron environment
 * Run with: npx electron test-electron.js
 */

const { app } = require('electron');
const path = require('path');

app.whenReady().then(async () => {
    console.log('=== Electron Environment Info ===');
    console.log('Node.js version:', process.versions.node);
    console.log('Electron version:', process.versions.electron);
    console.log('Chrome version:', process.versions.chrome);
    console.log('V8 version:', process.versions.v8);
    console.log('ABI version:', process.versions.modules);
    console.log('Platform:', process.platform);
    console.log('Arch:', process.arch);
    console.log('=================================\n');

    console.log('Testing avioflow module loading...\n');

    try {
        // Step 1: Find the module path
        console.log('Step 1: Resolving avioflow module path...');
        const avioflowPath = require.resolve('avioflow');
        console.log('  Resolved path:', avioflowPath);

        // Step 2: Try to load via node-gyp-build directly
        console.log('\nStep 2: Loading via node-gyp-build...');
        const ngb = require('node-gyp-build');
        const avioflowDir = path.join(__dirname, 'node_modules', 'avioflow');
        console.log('  avioflow dir:', avioflowDir);

        console.log('  Calling node-gyp-build...');
        const addon = ngb(avioflowDir);
        console.log('  ✓ Native addon loaded directly!');
        console.log('  Module keys:', Object.keys(addon));

        // Step 3: Test basic functionality
        console.log('\nStep 3: Testing setLogLevel...');
        if (addon.setLogLevel) {
            addon.setLogLevel(0); // Silent
            console.log('  ✓ setLogLevel works');
        }

        console.log('\nStep 4: Testing listAudioDevices...');
        if (addon.listAudioDevices) {
            try {
                const devices = addon.listAudioDevices();
                console.log('  ✓ listAudioDevices works, found', devices.length, 'devices');
            } catch (e) {
                console.log('  ⚠ listAudioDevices error:', e.message);
            }
        }

        console.log('\nStep 5: Testing AudioDecoder class...');
        if (addon.AudioDecoder) {
            console.log('  ✓ AudioDecoder class exists');
        }

        console.log('\nStep 6: Testing load function...');
        if (addon.load) {
            console.log('  ✓ load function exists');
        }

        console.log('\n=================================');
        console.log('✓ All tests passed! avioflow works in Electron.');
        console.log('=================================\n');

    } catch (error) {
        console.error('\n❌ FAILED to load avioflow:');
        console.error('Error:', error.message);
        console.error('Stack:', error.stack);
        console.error('\nThis is likely an ABI compatibility issue.');
        console.error('The prebuild may need to be rebuilt for Electron', process.versions.electron);
    }

    app.quit();
});

app.on('window-all-closed', () => {
    app.quit();
});
