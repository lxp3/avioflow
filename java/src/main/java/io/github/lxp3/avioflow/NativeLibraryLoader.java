package io.github.lxp3.avioflow;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.Locale;

final class NativeLibraryLoader {
    private static volatile boolean loaded;

    private NativeLibraryLoader() {
    }

    static synchronized void load() {
        if (loaded) {
            return;
        }

        String override = System.getProperty("avioflow.native.path");
        if (override == null || override.isEmpty()) {
            override = System.getenv("AVIOFLOW_JNI_PATH");
        }
        if (override != null && !override.isEmpty()) {
            System.load(override);
            loaded = true;
            return;
        }

        String classifier = classifier();
        String libraryName = System.mapLibraryName("avioflow_jni");
        String resource = "/io/github/lxp3/avioflow/native/" + classifier + "/" + libraryName;

        try (InputStream input = NativeLibraryLoader.class.getResourceAsStream(resource)) {
            if (input == null) {
                throw new AvioflowException(
                        "Native library not found for " + classifier
                                + ". Add runtime dependency io.github.lxp3:avioflow:<version>:" + classifier
                                + " or set AVIOFLOW_JNI_PATH.");
            }

            Path dir = Files.createTempDirectory("avioflow-" + classifier + "-");
            Path library = dir.resolve(libraryName);
            Files.copy(input, library, StandardCopyOption.REPLACE_EXISTING);
            library.toFile().deleteOnExit();
            dir.toFile().deleteOnExit();
            System.load(library.toAbsolutePath().toString());
            loaded = true;
        } catch (IOException e) {
            throw new AvioflowException("Failed to extract avioflow native library", e);
        }
    }

    static String classifier() {
        return normalizeOs(System.getProperty("os.name")) + "-" + normalizeArch(System.getProperty("os.arch"));
    }

    private static String normalizeOs(String osName) {
        String os = osName.toLowerCase(Locale.ROOT);
        if (os.contains("win")) {
            return "windows";
        }
        if (os.contains("mac") || os.contains("darwin")) {
            return "macos";
        }
        if (os.contains("linux")) {
            return "linux";
        }
        return os.replaceAll("[^a-z0-9]+", "");
    }

    private static String normalizeArch(String archName) {
        String arch = archName.toLowerCase(Locale.ROOT);
        if (arch.equals("x86_64") || arch.equals("amd64")) {
            return "x86_64";
        }
        if (arch.equals("aarch64") || arch.equals("arm64")) {
            return "aarch64";
        }
        return arch.replaceAll("[^a-z0-9_]+", "");
    }
}

