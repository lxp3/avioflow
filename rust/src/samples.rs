//! Conversion helpers between Rust planar samples and the C ABI.

use crate::error::{check, Result};
use crate::ffi;

/// Owns an `AvfSamples` handle and frees it on drop.
///
/// Only used internally: results are copied into `Vec<Vec<f32>>` before being
/// handed to callers, so no crate user has to think about the handle's lifetime.
pub(crate) struct OwnedSamples {
    handle: *mut ffi::AvfSamples,
}

impl OwnedSamples {
    /// Calls `f`, which must write an owned handle to its out-parameter on
    /// success, and takes ownership of the result.
    pub(crate) fn from_call<F>(f: F) -> Result<Self>
    where
        F: FnOnce(*mut *mut ffi::AvfSamples) -> std::os::raw::c_int,
    {
        let mut handle: *mut ffi::AvfSamples = std::ptr::null_mut();
        check(f(&mut handle))?;
        Ok(Self { handle })
    }

    /// Copies the samples into owned vectors, one per channel.
    pub(crate) fn to_vecs(&self) -> Vec<Vec<f32>> {
        if self.handle.is_null() {
            return Vec::new();
        }

        // SAFETY: `handle` is non-null and owned by self, so it stays valid for
        // this call. The accessors bound-check the channel index themselves and
        // report the exact length of the data they return.
        unsafe {
            let num_channels = ffi::avf_samples_num_channels(self.handle);
            let num_samples = ffi::avf_samples_num_samples(self.handle);
            if num_channels <= 0 || num_samples <= 0 {
                return Vec::new();
            }

            let len = num_samples as usize;
            let mut result = Vec::with_capacity(num_channels as usize);
            for channel in 0..num_channels {
                let data = ffi::avf_samples_channel(self.handle, channel);
                if data.is_null() {
                    result.push(Vec::new());
                } else {
                    result.push(std::slice::from_raw_parts(data, len).to_vec());
                }
            }
            result
        }
    }
}

impl Drop for OwnedSamples {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            // SAFETY: the handle came from the C API, is owned by self, and is
            // freed exactly once because Drop runs once and nothing else frees it.
            unsafe { ffi::avf_samples_free(self.handle) };
            self.handle = std::ptr::null_mut();
        }
    }
}

/// Validates planar input and builds the array of channel pointers the C ABI
/// expects.
///
/// The returned `Vec` borrows `samples`, so it must not outlive it. Returns the
/// pointer array plus the per-channel sample count.
pub(crate) fn channel_pointers(samples: &[Vec<f32>]) -> Result<(Vec<*const f32>, i64)> {
    if samples.is_empty() {
        return Ok((Vec::new(), 0));
    }

    let num_samples = samples[0].len();
    if let Some(index) = samples.iter().position(|c| c.len() != num_samples) {
        return Err(crate::error::Error::new(
            crate::error::ErrorKind::InvalidArgument,
            format!(
                "all channels must have the same length: channel 0 has {} samples, \
                 channel {index} has {}",
                num_samples,
                samples[index].len()
            ),
        ));
    }

    let pointers = samples.iter().map(|c| c.as_ptr()).collect();
    Ok((pointers, num_samples as i64))
}
