//! Stream metadata returned by the decoder.

use std::ffi::CStr;
use std::os::raw::c_char;

use crate::ffi;

/// Describes an audio stream.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct Metadata {
    /// Duration in seconds.
    pub duration: f64,
    /// Total sample count per channel, when known.
    pub num_samples: i64,
    /// Sample rate in Hz. Reflects `output_sample_rate` when resampling.
    pub sample_rate: i32,
    /// Channel count. Reflects `output_num_channels` when remixing.
    pub num_channels: i32,
    /// Bit rate in bits per second.
    pub bit_rate: i64,
    /// Sample format name, for example `"fltp"`.
    pub sample_format: String,
    /// Codec name, for example `"mp3"`.
    pub codec: String,
    /// Container format name, for example `"mp3"`.
    pub container: String,
}

impl Metadata {
    pub(crate) fn from_ffi(raw: &ffi::AvfMetadata) -> Self {
        Self {
            duration: raw.duration,
            num_samples: raw.num_samples,
            sample_rate: raw.sample_rate,
            num_channels: raw.num_channels,
            bit_rate: raw.bit_rate,
            sample_format: fixed_string(&raw.sample_format),
            codec: fixed_string(&raw.codec),
            container: fixed_string(&raw.container),
        }
    }
}

/// Reads a NUL-terminated string out of a fixed-size C char array.
fn fixed_string(buffer: &[c_char]) -> String {
    // SAFETY: the shim always NUL-terminates these buffers (see copy_string in
    // avioflow-c-api.cpp), and the pointer is to a live array borrowed here.
    unsafe {
        CStr::from_ptr(buffer.as_ptr())
            .to_string_lossy()
            .into_owned()
    }
}
