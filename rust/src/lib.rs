//! Rust bindings for [avioflow](https://github.com/lxp3/avioflow), a
//! high-performance audio library built on FFmpeg.
//!
//! The crate wraps the same C++ core the Python, Node.js, Java and WebAssembly
//! bindings use, reached through a thin C ABI. FFmpeg is compiled in statically,
//! so nothing has to be installed or located at runtime.
//!
//! Samples are always planar float: `samples[channel][sample]`, with every
//! channel the same length.
//!
//! # Decoding
//!
//! ```no_run
//! use avioflow::{AudioDecoder, StreamOptions};
//!
//! # fn main() -> Result<(), avioflow::Error> {
//! let mut decoder = AudioDecoder::new(&StreamOptions::new().output_sample_rate(16000))?;
//! let metadata = decoder.load_file("audio.mp3")?;
//! let samples = decoder.get_samples()?;
//!
//! println!("{:.1}s, {} Hz", metadata.duration, metadata.sample_rate);
//! println!("{} channels x {} samples", samples.len(), samples[0].len());
//! # Ok(())
//! # }
//! ```
//!
//! # Resampling
//!
//! ```no_run
//! # fn main() -> Result<(), avioflow::Error> {
//! # let samples: Vec<Vec<f32>> = vec![vec![0.0; 44100], vec![0.0; 44100]];
//! let mono_16k = avioflow::resample(&samples, 44100, 16000, Some(1))?;
//! # Ok(())
//! # }
//! ```
//!
//! # Encoding
//!
//! ```no_run
//! use avioflow::{save_audio, WriteOptions};
//!
//! # fn main() -> Result<(), avioflow::Error> {
//! # let samples: Vec<Vec<f32>> = vec![vec![0.0; 16000]];
//! save_audio("out.wav", &samples, &WriteOptions::new()
//!     .container_format("wav")
//!     .sample_rate(16000))?;
//! # Ok(())
//! # }
//! ```
//!
//! # Threading
//!
//! Handle types are [`Send`] but not [`Sync`]: move one to another thread
//! freely, but do not share a single decoder, encoder or resampler between
//! threads without external synchronization.

#![warn(missing_docs)]
#![deny(unsafe_op_in_unsafe_fn)]

// Compiles the examples in README.md as doctests so they cannot drift from the
// API. Only present during `cargo test`, so it adds nothing to the built crate.
#[cfg(doctest)]
#[doc = include_str!("../README.md")]
pub struct ReadmeDoctests;

mod decoder;
mod encoder;
mod error;
mod ffi;
mod metadata;
mod options;
mod resampler;
mod samples;

pub use decoder::{AudioDecoder, Frame};
pub use encoder::{save_audio, AudioEncoder};
pub use error::{Error, ErrorKind, Result};
pub use metadata::Metadata;
pub use options::{ResampleOptions, StreamOptions, WriteOptions};
pub use resampler::{resample, AudioResampler};

use std::ffi::CStr;

use crate::error::check;

/// An audio input or output device.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DeviceInfo {
    /// Identifier to pass to [`AudioDecoder::load_file`] to open this device.
    pub name: String,
    /// Human-readable description.
    pub description: String,
    /// Whether this is an output or loopback device.
    pub is_output: bool,
}

/// Sets the FFmpeg log level.
///
/// Accepts `"quiet"`, `"panic"`, `"fatal"`, `"error"`, `"warning"`, `"info"`,
/// `"verbose"`, `"debug"`, `"trace"`. Passing `None` restores the default,
/// which is `"error"` unless `AVIOFLOW_LOG_LEVEL` is set.
///
/// An unrecognized level is ignored by the native layer rather than reported.
pub fn set_log_level(level: Option<&str>) {
    match level {
        Some(value) => {
            // An interior NUL cannot name a real level, so treat it as unset
            // rather than failing a function with no error channel.
            if let Ok(text) = std::ffi::CString::new(value) {
                // SAFETY: text outlives the call and is NUL-terminated.
                unsafe { ffi::avf_set_log_level(text.as_ptr()) };
            }
        }
        // SAFETY: the C API documents NULL as "use the default".
        None => unsafe { ffi::avf_set_log_level(std::ptr::null()) },
    }
}

/// Names of the audio decoders this build supports, for example `"mp3"`.
pub fn supported_decoders() -> Result<Vec<String>> {
    // SAFETY: out points to a live local pointer.
    string_list(|out| unsafe { ffi::avf_get_supported_decoders(out) })
}

/// Names of the audio encoders this build supports, for example `"pcm_s16le"`.
pub fn supported_encoders() -> Result<Vec<String>> {
    // SAFETY: as above.
    string_list(|out| unsafe { ffi::avf_get_supported_encoders(out) })
}

/// Names of the input formats (demuxers) this build supports.
pub fn supported_input_formats() -> Result<Vec<String>> {
    // SAFETY: as above.
    string_list(|out| unsafe { ffi::avf_get_supported_input_formats(out) })
}

/// Names of the output formats (muxers) this build supports.
pub fn supported_output_formats() -> Result<Vec<String>> {
    // SAFETY: as above.
    string_list(|out| unsafe { ffi::avf_get_supported_output_formats(out) })
}

/// Enumerates available audio devices.
pub fn list_audio_devices() -> Result<Vec<DeviceInfo>> {
    let mut handle: *mut ffi::AvfDeviceList = std::ptr::null_mut();
    // SAFETY: handle is a live local the callee writes an owned pointer into.
    check(unsafe { ffi::avf_list_audio_devices(&mut handle) })?;
    if handle.is_null() {
        return Ok(Vec::new());
    }

    // SAFETY: handle is a valid owned list; the accessors bound-check `index`
    // and return NUL-terminated strings valid until the list is freed. The list
    // is freed once, below, on every path out of this function.
    let devices = unsafe {
        let count = ffi::avf_device_list_size(handle);
        let mut devices = Vec::with_capacity(count);
        for index in 0..count {
            devices.push(DeviceInfo {
                name: owned_string(ffi::avf_device_list_name(handle, index)),
                description: owned_string(ffi::avf_device_list_description(handle, index)),
                is_output: ffi::avf_device_list_is_output(handle, index) != 0,
            });
        }
        ffi::avf_device_list_free(handle);
        devices
    };

    Ok(devices)
}

/// Runs a call that yields an owned string list and copies it into `Vec<String>`.
fn string_list<F>(f: F) -> Result<Vec<String>>
where
    F: FnOnce(*mut *mut ffi::AvfStringList) -> std::os::raw::c_int,
{
    let mut handle: *mut ffi::AvfStringList = std::ptr::null_mut();
    check(f(&mut handle))?;
    if handle.is_null() {
        return Ok(Vec::new());
    }

    // SAFETY: as in list_audio_devices; the list is freed exactly once here.
    let items = unsafe {
        let count = ffi::avf_string_list_size(handle);
        let mut items = Vec::with_capacity(count);
        for index in 0..count {
            items.push(owned_string(ffi::avf_string_list_get(handle, index)));
        }
        ffi::avf_string_list_free(handle);
        items
    };

    Ok(items)
}

/// Copies a borrowed C string. Empty for null.
///
/// # Safety
///
/// `raw` must be null or point to a NUL-terminated string valid for this call.
unsafe fn owned_string(raw: *const std::os::raw::c_char) -> String {
    if raw.is_null() {
        return String::new();
    }
    unsafe { CStr::from_ptr(raw).to_string_lossy().into_owned() }
}
