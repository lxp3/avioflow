//! Builds the avioflow C++ core plus the C ABI shim in `csrc/`, then emits the
//! link flags needed to bind them into the crate.
//!
//! The CMake project is the repository root (the crate's parent directory). It
//! It is configured with static linkage throughout, so consumers of this crate
//! do not have to locate shared libraries at runtime. The C ABI the crate binds
//! to is part of the core library itself, so no separate binding target is
//! built here.

use std::collections::HashSet;
use std::env;
use std::fs;
use std::path::{Path, PathBuf};

/// FFmpeg libraries in dependency order. A static linker resolves symbols left
/// to right, so higher-level libraries must precede the ones they depend on;
/// avutil is last because everything else needs it.
const FFMPEG_LIBS: &[&str] = &[
    "avdevice",
    "avfilter",
    "avformat",
    "avcodec",
    "swscale",
    "swresample",
    "avutil",
];

/// Third-party libraries bundled alongside FFmpeg in the prebuilt package, in
/// dependency order and after all of FFmpeg. Which ones are present depends on
/// how that package was configured, so each is linked only if its archive
/// exists. vorbisenc precedes vorbis, and ssl precedes crypto, because each
/// depends on the next.
const BUNDLED_LIBS: &[&str] = &[
    "mp3lame",
    "opus",
    "speex",
    "vorbisenc",
    "vorbis",
    "ogg",
    "ssl",
    "crypto",
];

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let project_root = cmake_source_dir(&manifest_dir);

    rerun_if_changed(&manifest_dir, &project_root);

    let install_dir = build_native(&project_root);
    let lib_dir = install_dir.join("lib");

    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    // The core carries the C ABI and must precede FFmpeg, which it depends on.
    println!("cargo:rustc-link-lib=static=avioflow");

    for lib in FFMPEG_LIBS.iter().chain(BUNDLED_LIBS) {
        if lib_dir.join(format!("lib{lib}.a")).exists() {
            println!("cargo:rustc-link-lib=static={lib}");
        }
    }

    // After FFmpeg, because it resolves the __*_finite math symbols the bundled
    // libvorbis still references and a static linker resolves left to right.
    if lib_dir.join("libavioflow_glibc_compat.a").exists() {
        println!("cargo:rustc-link-lib=static=avioflow_glibc_compat");
    }

    // Anything left is expected to come from the host system, so link it
    // dynamically.
    for lib in system_libs(&lib_dir) {
        println!("cargo:rustc-link-lib={lib}");
    }

    if let Some(stdlib) = cxx_stdlib() {
        println!("cargo:rustc-link-lib={stdlib}");
    }

    // Expose the install prefix so dependent build scripts can find the header.
    println!("cargo:root={}", install_dir.display());
    println!("cargo:include={}", install_dir.join("include").display());
}

/// Locates the CMake project.
///
/// A published crate cannot reach outside its own directory, so the native
/// sources are staged into `native/` by `scripts/vendor_rust_sources.py` before
/// packaging. In a repository checkout that directory is absent and the CMake
/// project at the repository root is used directly, so no vendoring step is
/// needed for local development.
fn cmake_source_dir(manifest_dir: &Path) -> PathBuf {
    let vendored = manifest_dir.join("native");
    if vendored.join("CMakeLists.txt").exists() {
        println!("cargo:rerun-if-changed={}", vendored.display());
        return vendored;
    }

    let repo_root = manifest_dir
        .parent()
        .expect("crate directory must have a parent")
        .to_path_buf();

    if repo_root.join("CMakeLists.txt").exists() {
        return repo_root;
    }

    panic!(
        "Could not find the avioflow CMake project. Looked for {} and {}. \
         When building from a repository checkout, keep the crate in rust/; \
         when packaging, run scripts/vendor_rust_sources.py first.",
        vendored.join("CMakeLists.txt").display(),
        repo_root.join("CMakeLists.txt").display()
    );
}

fn rerun_if_changed(manifest_dir: &Path, project_root: &Path) {
    println!(
        "cargo:rerun-if-changed={}",
        manifest_dir.join("build.rs").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        project_root.join("CMakeLists.txt").display()
    );
    println!(
        "cargo:rerun-if-changed={}",
        project_root.join("avioflow").display()
    );
    println!("cargo:rerun-if-env-changed=AVIOFLOW_CMAKE_ARGS");
}

fn build_native(project_root: &Path) -> PathBuf {
    let mut config = cmake::Config::new(project_root);
    config
        .define("BUILD_SHARED_LIBS", "OFF")
        .define("BUILD_TESTS", "OFF")
        .define("BUILD_TESTING", "OFF")
        .define("ENABLE_BINARY", "OFF")
        .define("ENABLE_PYTHON", "OFF")
        .define("ENABLE_NODE_JS", "OFF")
        .define("ENABLE_JAVA", "OFF")
        // A Debug Rust profile should not force a Debug C++ core; the core is
        // an opaque dependency and building it optimized is always preferable.
        .profile("Release");

    if let Ok(extra) = env::var("AVIOFLOW_CMAKE_ARGS") {
        for arg in extra.split_whitespace() {
            if let Some((key, value)) = arg.trim_start_matches("-D").split_once('=') {
                config.define(key, value);
            }
        }
    }

    config.build()
}

/// Collects the host-provided libraries FFmpeg needs.
///
/// Read from the `Libs` and `Libs.private` lines of FFmpeg's pkg-config files
/// rather than hardcoded, because the exact set depends on how the FFmpeg
/// package was configured. Names that resolve to an archive in `lib_dir` are
/// skipped: those are bundled and already linked statically, in a deliberate
/// order, by the caller.
fn system_libs(lib_dir: &Path) -> Vec<String> {
    let mut libs = Vec::new();
    let mut seen: HashSet<String> = HashSet::new();

    // Handled explicitly, in dependency order, before this runs.
    for known in FFMPEG_LIBS.iter().chain(BUNDLED_LIBS) {
        seen.insert((*known).to_string());
    }

    let pkgconfig_dir = lib_dir.join("pkgconfig");
    if let Ok(entries) = fs::read_dir(&pkgconfig_dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.extension().and_then(|e| e.to_str()) != Some("pc") {
                continue;
            }
            let Ok(contents) = fs::read_to_string(&path) else {
                continue;
            };
            for line in contents.lines() {
                let rest = match line.split_once(':') {
                    Some(("Libs" | "Libs.private", rest)) => rest,
                    _ => continue,
                };
                for token in rest.split_whitespace() {
                    let Some(name) = token.strip_prefix("-l") else {
                        continue;
                    };
                    // A bundled archive is linked statically, not from the host.
                    if lib_dir.join(format!("lib{name}.a")).exists() {
                        continue;
                    }
                    if seen.insert(name.to_string()) {
                        libs.push(name.to_string());
                    }
                }
            }
        }
    }

    // Needed regardless of what the .pc files happen to mention.
    for fallback in ["m", "pthread", "dl"] {
        if seen.insert(fallback.to_string()) {
            libs.push(fallback.to_string());
        }
    }

    libs
}

fn cxx_stdlib() -> Option<&'static str> {
    let target = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    match target.as_str() {
        "macos" | "ios" => Some("c++"),
        // MSVC links the C++ runtime automatically.
        "windows" => None,
        _ => Some("stdc++"),
    }
}
