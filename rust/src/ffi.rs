//! Raw declarations for the C ABI in `avioflow/include/avioflow-c-api.h`.
//!
//! This module is a mechanical 1:1 transcription of that header and carries no
//! safety logic of its own. Everything here is `unsafe` to call; the safe
//! wrappers live in the sibling modules.

use std::os::raw::{c_char, c_int};

pub const AVF_OK: c_int = 0;

// Opaque handle types. Only ever held behind a pointer.
#[repr(C)]
pub struct AvfDecoder {
    _private: [u8; 0],
}

#[repr(C)]
pub struct AvfEncoder {
    _private: [u8; 0],
}

#[repr(C)]
pub struct AvfResampler {
    _private: [u8; 0],
}

#[repr(C)]
pub struct AvfSamples {
    _private: [u8; 0],
}

#[repr(C)]
pub struct AvfStringList {
    _private: [u8; 0],
}

#[repr(C)]
pub struct AvfDeviceList {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct AvfMetadata {
    pub duration: f64,
    pub num_samples: i64,
    pub sample_rate: i32,
    pub num_channels: i32,
    pub bit_rate: i64,
    pub sample_format: [c_char; 32],
    pub codec: [c_char; 64],
    pub container: [c_char; 64],
}

impl Default for AvfMetadata {
    fn default() -> Self {
        // All fields are plain data, so a zeroed struct is a valid empty value.
        unsafe { std::mem::zeroed() }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct AvfStreamOptions {
    pub output_sample_rate: i32,
    pub has_output_sample_rate: i32,
    pub output_num_channels: i32,
    pub has_output_num_channels: i32,
    pub input_sample_rate: i32,
    pub has_input_sample_rate: i32,
    pub input_channels: i32,
    pub has_input_channels: i32,
    pub input_format: *const c_char,
}

impl Default for AvfStreamOptions {
    fn default() -> Self {
        Self {
            output_sample_rate: 0,
            has_output_sample_rate: 0,
            output_num_channels: 0,
            has_output_num_channels: 0,
            input_sample_rate: 0,
            has_input_sample_rate: 0,
            input_channels: 0,
            has_input_channels: 0,
            input_format: std::ptr::null(),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct AvfWriteOptions {
    pub codec_name: *const c_char,
    pub container_format: *const c_char,
    pub sample_format: *const c_char,
    pub sample_rate: i32,
    pub has_sample_rate: i32,
    pub num_channels: i32,
    pub has_num_channels: i32,
    pub bit_rate: i64,
    pub has_bit_rate: i32,
    pub overwrite: i32,
}

impl Default for AvfWriteOptions {
    fn default() -> Self {
        Self {
            codec_name: std::ptr::null(),
            container_format: std::ptr::null(),
            sample_format: std::ptr::null(),
            sample_rate: 0,
            has_sample_rate: 0,
            num_channels: 0,
            has_num_channels: 0,
            bit_rate: 0,
            has_bit_rate: 0,
            // Matches AudioWriteOptions::overwrite, which defaults to true.
            overwrite: 1,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct AvfResampleOptions {
    pub input_sample_rate: i32,
    pub output_sample_rate: i32,
    pub output_num_channels: i32,
    pub has_output_num_channels: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct AvfFrame {
    pub data: *const *const f32,
    pub num_channels: i32,
    pub num_samples: i32,
}

impl Default for AvfFrame {
    fn default() -> Self {
        Self {
            data: std::ptr::null(),
            num_channels: 0,
            num_samples: 0,
        }
    }
}

extern "C" {
    pub fn avf_last_error() -> *const c_char;
    pub fn avf_last_error_code() -> c_int;
    pub fn avf_set_log_level(level: *const c_char);

    pub fn avf_string_list_size(list: *const AvfStringList) -> usize;
    pub fn avf_string_list_get(list: *const AvfStringList, index: usize) -> *const c_char;
    pub fn avf_string_list_free(list: *mut AvfStringList);

    pub fn avf_get_supported_decoders(out_list: *mut *mut AvfStringList) -> c_int;
    pub fn avf_get_supported_encoders(out_list: *mut *mut AvfStringList) -> c_int;
    pub fn avf_get_supported_input_formats(out_list: *mut *mut AvfStringList) -> c_int;
    pub fn avf_get_supported_output_formats(out_list: *mut *mut AvfStringList) -> c_int;

    pub fn avf_samples_num_channels(samples: *const AvfSamples) -> i32;
    pub fn avf_samples_num_samples(samples: *const AvfSamples) -> i64;
    pub fn avf_samples_channel(samples: *const AvfSamples, channel: i32) -> *const f32;
    pub fn avf_samples_free(samples: *mut AvfSamples);

    pub fn avf_decoder_new(options: *const AvfStreamOptions) -> *mut AvfDecoder;
    pub fn avf_decoder_free(decoder: *mut AvfDecoder);
    pub fn avf_decoder_load_file(
        decoder: *mut AvfDecoder,
        source: *const c_char,
        out_metadata: *mut AvfMetadata,
    ) -> c_int;
    pub fn avf_decoder_load_buffer(
        decoder: *mut AvfDecoder,
        data: *const u8,
        size: usize,
        out_metadata: *mut AvfMetadata,
    ) -> c_int;
    pub fn avf_decoder_feed(decoder: *mut AvfDecoder, data: *const u8, size: usize) -> c_int;
    pub fn avf_decoder_flush(decoder: *mut AvfDecoder) -> c_int;
    pub fn avf_decoder_get_frame(decoder: *mut AvfDecoder, out_frame: *mut AvfFrame) -> c_int;
    pub fn avf_decoder_get_samples(
        decoder: *mut AvfDecoder,
        start_seconds: f64,
        stop_seconds: f64,
        has_stop_seconds: i32,
        out_samples: *mut *mut AvfSamples,
    ) -> c_int;
    pub fn avf_decoder_is_finished(decoder: *const AvfDecoder, out_finished: *mut i32) -> c_int;
    pub fn avf_decoder_get_metadata(
        decoder: *const AvfDecoder,
        out_metadata: *mut AvfMetadata,
    ) -> c_int;

    pub fn avf_encoder_new(options: *const AvfWriteOptions) -> *mut AvfEncoder;
    pub fn avf_encoder_free(encoder: *mut AvfEncoder);
    pub fn avf_encoder_save(
        encoder: *mut AvfEncoder,
        path: *const c_char,
        channels: *const *const f32,
        num_channels: i32,
        num_samples: i64,
    ) -> c_int;
    pub fn avf_encoder_save_buffer(
        encoder: *mut AvfEncoder, channels: *const *const f32, num_channels: i32,
        num_samples: i64, out_data: *mut *mut u8, out_size: *mut usize) -> c_int;
    pub fn avf_free_buffer(data: *mut u8);
    pub fn avf_save_audio(
        path: *const c_char,
        channels: *const *const f32,
        num_channels: i32,
        num_samples: i64,
        options: *const AvfWriteOptions,
    ) -> c_int;

    pub fn avf_resampler_new(options: *const AvfResampleOptions) -> *mut AvfResampler;
    pub fn avf_resampler_free(resampler: *mut AvfResampler);
    pub fn avf_resampler_process(
        resampler: *mut AvfResampler,
        channels: *const *const f32,
        num_channels: i32,
        num_samples: i64,
        out_samples: *mut *mut AvfSamples,
    ) -> c_int;
    pub fn avf_resampler_flush(
        resampler: *mut AvfResampler,
        out_samples: *mut *mut AvfSamples,
    ) -> c_int;
    pub fn avf_resampler_output_sample_rate(
        resampler: *const AvfResampler,
        out_rate: *mut i32,
    ) -> c_int;
    pub fn avf_resampler_output_num_channels(
        resampler: *const AvfResampler,
        out_channels: *mut i32,
    ) -> c_int;
    pub fn avf_resample(
        channels: *const *const f32,
        num_channels: i32,
        num_samples: i64,
        input_sample_rate: i32,
        output_sample_rate: i32,
        output_num_channels: i32,
        has_output_num_channels: i32,
        out_samples: *mut *mut AvfSamples,
    ) -> c_int;

    pub fn avf_list_audio_devices(out_list: *mut *mut AvfDeviceList) -> c_int;
    pub fn avf_device_list_size(list: *const AvfDeviceList) -> usize;
    pub fn avf_device_list_name(list: *const AvfDeviceList, index: usize) -> *const c_char;
    pub fn avf_device_list_description(list: *const AvfDeviceList, index: usize) -> *const c_char;
    pub fn avf_device_list_is_output(list: *const AvfDeviceList, index: usize) -> i32;
    pub fn avf_device_list_free(list: *mut AvfDeviceList);
}
