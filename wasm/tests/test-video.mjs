/**
 * Node.js test for audio extraction from video in the avioflow WASM module.
 *
 * Covers stream listing, audio track selection, readable error messages, and
 * decoding a mounted real file (NODEFS) without copying it into the WASM heap.
 *
 * Needs ffmpeg on PATH to generate the fixture; the test is skipped without it.
 * Run with: node tests/test-video.mjs
 */
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

if (spawnSync('ffmpeg', ['-version']).status !== 0) {
    console.log('SKIP: ffmpeg not found, cannot generate the video fixture');
    process.exit(0);
}

const dir = mkdtempSync(join(tmpdir(), 'avioflow-wasm-video-'));
const cleanup = () => rmSync(dir, { recursive: true, force: true });

try {
    // Video + 44.1 kHz "eng" audio + 48 kHz "jpn" audio, 4 seconds.
    execFileSync('ffmpeg', [
        '-loglevel', 'error', '-y',
        '-f', 'lavfi', '-i', 'testsrc=d=4:s=160x120:r=25',
        '-f', 'lavfi', '-i', 'sine=f=440:d=4:r=44100',
        '-f', 'lavfi', '-i', 'sine=f=880:d=4:r=48000',
        '-map', '0', '-map', '1', '-map', '2',
        '-c:v', 'mpeg4', '-c:a', 'aac',
        '-metadata:s:a:0', 'language=eng', '-metadata:s:a:1', 'language=jpn',
        join(dir, 'multi.mp4'),
    ]);

    const createAvioflow = (await import('../dist/avioflow.js')).default;
    const m = await createAvioflow();

    // Mount the directory so loadFile() reads on demand instead of the whole
    // file being copied into the WASM heap first.
    m.FS.mkdir('/mnt');
    m.FS.mount(m.FS.filesystems.NODEFS, { root: dir }, '/mnt');
    const path = '/mnt/multi.mp4';

    // getStreams() is empty before load and lists every stream afterwards.
    const decoder = new m.AudioDecoder();
    assert.equal(decoder.getStreams().length, 0);
    decoder.loadFile(path);
    const streams = decoder.getStreams();
    assert.deepEqual(streams.map((s) => [s.index, s.mediaType]),
        [[0, 'video'], [1, 'audio'], [2, 'audio']]);
    assert.equal(streams[1].language, 'eng');
    assert.equal(streams[2].language, 'jpn');
    assert.equal(streams[2].sampleRate, 48000);
    console.log('PASS: getStreams');

    // audioStreamIndex picks the track.
    const second = new m.AudioDecoder({ audioStreamIndex: 2 });
    assert.equal(second.loadFile(path).sampleRate, 48000);
    console.log('PASS: audioStreamIndex selects the 48 kHz track');

    // A video stream index is rejected, and the message is readable.
    let message = '';
    try {
        new m.AudioDecoder({ audioStreamIndex: 0 }).loadFile(path);
    } catch (e) {
        message = typeof e === 'number' ? m.getExceptionMessage(e).join(' ') : String(e.message);
    }
    assert.match(message, /not an audio stream/);
    console.log('PASS: invalid stream index reports a readable error');

    // Frame-by-frame decode of the mounted file covers the whole duration.
    const frames = new m.AudioDecoder();
    const meta = frames.loadFile(path);
    let samples = 0;
    for (let f = frames.getFrame(); f; f = frames.getFrame()) samples += f[0].length;
    assert.ok(Math.abs(samples / meta.sampleRate - 4) < 0.1, `got ${samples} samples`);
    console.log('PASS: getFrame decodes the full mounted file');

    // A time range decodes exactly the requested length.
    const ranged = new m.AudioDecoder();
    ranged.loadFile(path);
    const slice = ranged.getSamples(1.0, 2.0);
    assert.equal(slice[0].length, ranged.getMetadata().sampleRate);
    console.log('PASS: getSamples(1, 2) returns one second');

    console.log('\nAll video tests passed');
} finally {
    cleanup();
}
