package io.github.lxp3.avioflow;

import java.util.Objects;

public final class AudioEncoder implements AutoCloseable {
    private long handle;

    public AudioEncoder() {
        this(null);
    }

    public AudioEncoder(AudioWriteOptions options) {
        NativeLibraryLoader.load();
        this.handle = nativeCreate(Avioflow.writeOptionsOrDefault(options));
    }

    public synchronized void save(String path, float[][] samples) {
        ensureOpen();
        nativeSave(handle, Objects.requireNonNull(path, "path"), Objects.requireNonNull(samples, "samples"));
    }

    @Override
    public synchronized void close() {
        long current = handle;
        if (current != 0) {
            handle = 0;
            nativeDestroy(current);
        }
    }

    public static void saveAudio(String path, float[][] samples, AudioWriteOptions options) {
        try (AudioEncoder encoder = new AudioEncoder(options)) {
            encoder.save(path, samples);
        }
    }

    private void ensureOpen() {
        if (handle == 0) {
            throw new IllegalStateException("AudioEncoder is closed");
        }
    }

    private static native long nativeCreate(AudioWriteOptions options);

    private static native void nativeSave(long handle, String path, float[][] samples);

    private static native void nativeDestroy(long handle);
}
