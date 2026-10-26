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

/// Describes one stream of an opened container: audio, video, subtitles, ...
#[derive(Debug, Clone, Default, PartialEq)]
pub struct StreamInfo {
    /// Value to pass to [`StreamOptions::audio_stream_index`](crate::StreamOptions::audio_stream_index).
    pub index: i32,
    /// `"audio"`, `"video"`, `"subtitle"`, `"data"` or `"attachment"`.
    pub media_type: String,
    /// Codec name, for example `"aac"` or `"h264"`.
    pub codec: String,
    /// ISO 639 language tag from the container. May be empty.
    pub language: String,
    /// Sample rate in Hz. Audio streams only, otherwise zero.
    pub sample_rate: i32,
    /// Channel count. Audio streams only, otherwise zero.
    pub num_channels: i32,
    /// Bit rate in bits per second. Zero if unknown.
    pub bit_rate: i64,
    /// Duration in seconds. Zero if unknown.
    pub duration: f64,
    /// Whether the container flags this as the default stream.
    pub is_default: bool,
}

impl StreamInfo {
    pub(crate) fn from_ffi(raw: &ffi::AvfStreamInfo) -> Self {
        Self {
            index: raw.index,
            media_type: fixed_string(&raw.media_type),
            codec: fixed_string(&raw.codec),
            language: fixed_string(&raw.language),
            sample_rate: raw.sample_rate,
            num_channels: raw.num_channels,
            bit_rate: raw.bit_rate,
            duration: raw.duration,
            is_default: raw.is_default != 0,
        }
    }
}
