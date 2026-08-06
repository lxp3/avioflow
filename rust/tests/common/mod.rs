//! Shared test fixtures.
//!
//! Each integration test is its own binary and uses only part of this module,
//! so unused items here are expected rather than a sign of dead code.
#![allow(dead_code)]

use std::path::PathBuf;

/// Known properties of `public/wavs/TownTheme.mp3`, matching the constants in
/// `avioflow/bin/decoder-resample-test.cpp`.
pub const SAMPLE_RATE: i32 = 44100;
pub const NUM_CHANNELS: usize = 2;
pub const NUM_SAMPLES: usize = 4_297_722;

/// Only rate-ratio rounding should move a resampled count. A larger gap means
/// samples are being dropped. Mirrors SAMPLE_COUNT_TOLERANCE in the C++ tests.
pub const SAMPLE_COUNT_TOLERANCE: i64 = 2;

/// Absolute path to the checked-in test audio file.
pub fn test_audio_path() -> PathBuf {
    repo_root().join("public/wavs/TownTheme.mp3")
}

/// Repository root, resolved from the crate directory rather than the process
/// working directory so tests work regardless of how cargo is invoked.
pub fn repo_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("crate lives in a subdirectory of the repository")
        .to_path_buf()
}

/// Asserts `actual` is within [`SAMPLE_COUNT_TOLERANCE`] of `expected`.
pub fn assert_sample_count(actual: usize, expected: i64, context: &str) {
    let diff = actual as i64 - expected;
    assert!(
        diff.abs() <= SAMPLE_COUNT_TOLERANCE,
        "{context}: expected ~{expected} samples, got {actual} (diff {diff})"
    );
}
