//! Builder-style options mirroring the C++ option structs.
//!
//! Each type keeps its strings as owned `String`s and converts to the `repr(C)`
//! form only for the duration of a call, so no borrowed pointer outlives it.

use std::ffi::CString;

use crate::error::{to_cstring, Result};
use crate::ffi;

/// Options for [`AudioDecoder`](crate::AudioDecoder).
///
/// ```
/// use avioflow::StreamOptions;
/// let options = StreamOptions::new().output_sample_rate(16000).output_num_channels(1);
/// ```
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct StreamOptions {
    output_sample_rate: Option<i32>,
    output_num_channels: Option<i32>,
    input_sample_rate: Option<i32>,
    input_channels: Option<i32>,
    input_format: Option<String>,
}

impl StreamOptions {
    /// Options with every field unset, preserving the source stream's format.
    pub fn new() -> Self {
        Self::default()
    }

    /// Resample decoded audio to this rate. Unset preserves the source rate.
    pub fn output_sample_rate(mut self, rate: i32) -> Self {
        self.output_sample_rate = Some(rate);
        self
    }

    /// Remix decoded audio to this channel count. Unset preserves the source.
    pub fn output_num_channels(mut self, channels: i32) -> Self {
        self.output_num_channels = Some(channels);
        self
    }

    /// Sample rate of raw PCM stream input. Required for raw PCM streaming.
    pub fn input_sample_rate(mut self, rate: i32) -> Self {
        self.input_sample_rate = Some(rate);
        self
    }

    /// Channel count of raw PCM stream input. Required for raw PCM streaming.
    pub fn input_channels(mut self, channels: i32) -> Self {
        self.input_channels = Some(channels);
        self
    }

    /// Input format name, for example `"s16le"` or `"mp3"`. Required to use
    /// [`AudioDecoder::feed`](crate::AudioDecoder::feed).
    pub fn input_format(mut self, format: impl Into<String>) -> Self {
        self.input_format = Some(format.into());
        self
    }

    /// Builds the C representation. The returned `CString` owns the storage the
    /// struct's `input_format` pointer refers to and must outlive the call.
    pub(crate) fn to_ffi(&self) -> Result<(ffi::AvfStreamOptions, Option<CString>)> {
        let format = match &self.input_format {
            Some(value) => Some(to_cstring(value)?),
            None => None,
        };

        let mut raw = ffi::AvfStreamOptions {
            input_format: format.as_ref().map_or(std::ptr::null(), |c| c.as_ptr()),
            ..Default::default()
        };

        if let Some(value) = self.output_sample_rate {
            raw.output_sample_rate = value;
            raw.has_output_sample_rate = 1;
        }
        if let Some(value) = self.output_num_channels {
            raw.output_num_channels = value;
            raw.has_output_num_channels = 1;
        }
        if let Some(value) = self.input_sample_rate {
            raw.input_sample_rate = value;
            raw.has_input_sample_rate = 1;
        }
        if let Some(value) = self.input_channels {
            raw.input_channels = value;
            raw.has_input_channels = 1;
        }

        Ok((raw, format))
    }
}

/// Options for [`AudioEncoder`](crate::AudioEncoder) and
/// [`save_audio`](crate::save_audio).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct WriteOptions {
    codec_name: Option<String>,
    container_format: Option<String>,
    sample_format: Option<String>,
    sample_rate: Option<i32>,
    num_channels: Option<i32>,
    bit_rate: Option<i64>,
    overwrite: bool,
}

impl Default for WriteOptions {
    fn default() -> Self {
        Self {
            codec_name: None,
            container_format: None,
            sample_format: None,
            sample_rate: None,
            num_channels: None,
            bit_rate: None,
            // Matches the C++ AudioWriteOptions default.
            overwrite: true,
        }
    }
}

impl WriteOptions {
    /// Options with format fields unset and `overwrite` enabled. Unset fields
    /// are inferred by the encoder from the container and the input samples.
    pub fn new() -> Self {
        Self::default()
    }

    /// Codec name, for example `"pcm_s16le"`, `"flac"`, `"aac"`, `"libmp3lame"`.
    pub fn codec_name(mut self, codec: impl Into<String>) -> Self {
        self.codec_name = Some(codec.into());
        self
    }

    /// Container format, for example `"wav"`, `"flac"`, `"mp4"`, `"ogg"`.
    pub fn container_format(mut self, container: impl Into<String>) -> Self {
        self.container_format = Some(container.into());
        self
    }

    /// Sample format, for example `"s16"`, `"s32"`, `"flt"`, `"fltp"`.
    pub fn sample_format(mut self, format: impl Into<String>) -> Self {
        self.sample_format = Some(format.into());
        self
    }

    /// Output sample rate in Hz, for example 16000 or 44100.
    pub fn sample_rate(mut self, rate: i32) -> Self {
        self.sample_rate = Some(rate);
        self
    }

    /// Output channel count: 1 for mono, 2 for stereo.
    pub fn num_channels(mut self, channels: i32) -> Self {
        self.num_channels = Some(channels);
        self
    }

    /// Bit rate in bits per second, for lossy codecs.
    pub fn bit_rate(mut self, bit_rate: i64) -> Self {
        self.bit_rate = Some(bit_rate);
        self
    }

    /// Whether an existing file may be replaced. Defaults to `true`.
    pub fn overwrite(mut self, overwrite: bool) -> Self {
        self.overwrite = overwrite;
        self
    }

    pub(crate) fn to_ffi(&self) -> Result<(ffi::AvfWriteOptions, WriteOptionStrings)> {
        let strings = WriteOptionStrings {
            codec_name: self.codec_name.as_deref().map(to_cstring).transpose()?,
            container_format: self
                .container_format
                .as_deref()
                .map(to_cstring)
                .transpose()?,
            sample_format: self.sample_format.as_deref().map(to_cstring).transpose()?,
        };

        let mut raw = ffi::AvfWriteOptions {
            codec_name: strings.ptr(&strings.codec_name),
            container_format: strings.ptr(&strings.container_format),
            sample_format: strings.ptr(&strings.sample_format),
            overwrite: i32::from(self.overwrite),
            ..Default::default()
        };

        if let Some(value) = self.sample_rate {
            raw.sample_rate = value;
            raw.has_sample_rate = 1;
        }
        if let Some(value) = self.num_channels {
            raw.num_channels = value;
            raw.has_num_channels = 1;
        }
        if let Some(value) = self.bit_rate {
            raw.bit_rate = value;
            raw.has_bit_rate = 1;
        }

        Ok((raw, strings))
    }
}

/// Keeps the C strings referenced by [`ffi::AvfWriteOptions`] alive.
pub(crate) struct WriteOptionStrings {
    codec_name: Option<CString>,
    container_format: Option<CString>,
    sample_format: Option<CString>,
}

impl WriteOptionStrings {
    fn ptr(&self, value: &Option<CString>) -> *const std::os::raw::c_char {
        value.as_ref().map_or(std::ptr::null(), |c| c.as_ptr())
    }
}

/// Options for [`AudioResampler`](crate::AudioResampler).
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ResampleOptions {
    input_sample_rate: i32,
    output_sample_rate: i32,
    output_num_channels: Option<i32>,
}

impl ResampleOptions {
    /// Both rates are required and must be greater than zero.
    pub fn new(input_sample_rate: i32, output_sample_rate: i32) -> Self {
        Self {
            input_sample_rate,
            output_sample_rate,
            output_num_channels: None,
        }
    }

    /// Remix to this channel count. Unset keeps the input channel count.
    pub fn output_num_channels(mut self, channels: i32) -> Self {
        self.output_num_channels = Some(channels);
        self
    }

    pub(crate) fn to_ffi(&self) -> ffi::AvfResampleOptions {
        let mut raw = ffi::AvfResampleOptions {
            input_sample_rate: self.input_sample_rate,
            output_sample_rate: self.output_sample_rate,
            ..Default::default()
        };
        if let Some(value) = self.output_num_channels {
            raw.output_num_channels = value;
            raw.has_output_num_channels = 1;
        }
        raw
    }
}
