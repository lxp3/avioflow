//! Sample rate conversion and channel remixing.

use std::marker::PhantomData;

use crate::error::{check, null_handle_error, Result};
use crate::ffi;
use crate::options::ResampleOptions;
use crate::samples::{channel_pointers, OwnedSamples};

/// Stateful resampler for audio that arrives in chunks.
///
/// Filter state is preserved across [`process`](Self::process) calls, so
/// consecutive chunks join without discontinuities at the boundaries. For a
/// buffer you already hold in full, use [`resample`] instead.
///
/// [`flush`](Self::flush) is not optional: the resampler holds back the last few
/// milliseconds of audio internally, and skipping the flush discards them.
///
/// # Examples
///
/// ```no_run
/// use avioflow::{AudioResampler, ResampleOptions};
///
/// # fn main() -> Result<(), avioflow::Error> {
/// # let chunks: Vec<Vec<Vec<f32>>> = Vec::new();
/// let mut resampler = AudioResampler::new(&ResampleOptions::new(44100, 16000))?;
/// let mut output: Vec<Vec<f32>> = Vec::new();
///
/// for chunk in &chunks {
///     let part = resampler.process(chunk)?;
///     append(&mut output, part);
/// }
/// append(&mut output, resampler.flush()?); // else the tail is lost
///
/// fn append(output: &mut Vec<Vec<f32>>, part: Vec<Vec<f32>>) {
///     if output.is_empty() {
///         output.resize(part.len(), Vec::new());
///     }
///     for (channel, data) in output.iter_mut().zip(part) {
///         channel.extend(data);
///     }
/// }
/// # Ok(())
/// # }
/// ```
pub struct AudioResampler {
    handle: *mut ffi::AvfResampler,
    _not_sync: PhantomData<std::cell::Cell<()>>,
}

impl AudioResampler {
    /// Creates a resampler. Both sample rates in `options` must be positive.
    pub fn new(options: &ResampleOptions) -> Result<Self> {
        let raw = options.to_ffi();
        // SAFETY: `raw` is a fully initialized local that outlives the call.
        let handle = unsafe { ffi::avf_resampler_new(&raw) };
        if handle.is_null() {
            return Err(null_handle_error());
        }
        Ok(Self {
            handle,
            _not_sync: PhantomData,
        })
    }

    /// Resamples one chunk of `samples[channel][sample]`.
    ///
    /// May return fewer samples than the rate ratio suggests; the remainder is
    /// held until [`flush`](Self::flush). All channels must have the same
    /// length, and the channel count must not change between calls.
    pub fn process(&mut self, samples: &[Vec<f32>]) -> Result<Vec<Vec<f32>>> {
        let (pointers, num_samples) = channel_pointers(samples)?;
        let owned = OwnedSamples::from_call(|out| {
            // SAFETY: handle is valid; pointers outlives the call and has
            // exactly the reported number of entries.
            unsafe {
                ffi::avf_resampler_process(
                    self.handle,
                    pointers.as_ptr(),
                    pointers.len() as i32,
                    num_samples,
                    out,
                )
            }
        })?;
        Ok(owned.to_vecs())
    }

    /// Drains the samples still buffered inside the resampler.
    ///
    /// Call once after the final [`process`](Self::process).
    pub fn flush(&mut self) -> Result<Vec<Vec<f32>>> {
        let owned = OwnedSamples::from_call(|out| {
            // SAFETY: handle is valid; out points to a live local.
            unsafe { ffi::avf_resampler_flush(self.handle, out) }
        })?;
        Ok(owned.to_vecs())
    }

    /// Configured output sample rate in Hz.
    pub fn output_sample_rate(&self) -> Result<i32> {
        let mut rate = 0;
        // SAFETY: handle is valid; rate is a live local.
        check(unsafe { ffi::avf_resampler_output_sample_rate(self.handle, &mut rate) })?;
        Ok(rate)
    }

    /// Output channel count. Zero until the first [`process`](Self::process)
    /// call determines the input channel count.
    pub fn output_num_channels(&self) -> Result<i32> {
        let mut channels = 0;
        // SAFETY: handle is valid; channels is a live local.
        check(unsafe { ffi::avf_resampler_output_num_channels(self.handle, &mut channels) })?;
        Ok(channels)
    }
}

impl std::fmt::Debug for AudioResampler {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("AudioResampler")
            .field("output_sample_rate", &self.output_sample_rate().ok())
            .field("output_num_channels", &self.output_num_channels().ok())
            .finish()
    }
}

impl Drop for AudioResampler {
    fn drop(&mut self) {
        // SAFETY: handle came from avf_resampler_new and is freed exactly once.
        unsafe { ffi::avf_resampler_free(self.handle) };
    }
}

// SAFETY: see the corresponding impl on AudioDecoder.
unsafe impl Send for AudioResampler {}

/// Resamples a complete buffer in one call, flushing internally so no samples
/// are lost.
///
/// Pass `None` for `output_num_channels` to keep the input channel count.
///
/// For audio arriving in chunks use [`AudioResampler`]: calling this per chunk
/// would reset filter state and introduce a discontinuity at every boundary.
///
/// # Examples
///
/// ```no_run
/// use avioflow::resample;
///
/// # fn main() -> Result<(), avioflow::Error> {
/// # let samples: Vec<Vec<f32>> = vec![vec![0.0; 44100], vec![0.0; 44100]];
/// let downsampled = resample(&samples, 44100, 16000, None)?;
/// let mono = resample(&samples, 44100, 16000, Some(1))?;
/// # Ok(())
/// # }
/// ```
pub fn resample(
    samples: &[Vec<f32>],
    input_sample_rate: i32,
    output_sample_rate: i32,
    output_num_channels: Option<i32>,
) -> Result<Vec<Vec<f32>>> {
    let (pointers, num_samples) = channel_pointers(samples)?;
    let owned = OwnedSamples::from_call(|out| {
        // SAFETY: pointers outlives the call and has exactly the reported
        // number of entries, each valid for num_samples floats.
        unsafe {
            ffi::avf_resample(
                pointers.as_ptr(),
                pointers.len() as i32,
                num_samples,
                input_sample_rate,
                output_sample_rate,
                output_num_channels.unwrap_or(0),
                i32::from(output_num_channels.is_some()),
                out,
            )
        }
    })?;
    Ok(owned.to_vecs())
}
