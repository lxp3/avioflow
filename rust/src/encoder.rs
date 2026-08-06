//! Audio encoding.

use std::marker::PhantomData;

use crate::error::{check, null_handle_error, to_cstring, Result};
use crate::ffi;
use crate::options::WriteOptions;
use crate::samples::channel_pointers;

/// Writes in-memory planar float samples to an audio file.
///
/// For a single write, [`save_audio`] is more direct.
///
/// # Examples
///
/// ```no_run
/// use avioflow::{AudioEncoder, WriteOptions};
///
/// # fn main() -> Result<(), avioflow::Error> {
/// let samples = vec![vec![0.0f32; 16000]];
/// let mut encoder = AudioEncoder::new(&WriteOptions::new()
///     .container_format("wav")
///     .codec_name("pcm_s16le")
///     .sample_rate(16000))?;
/// encoder.save("out.wav", &samples)?;
/// # Ok(())
/// # }
/// ```
pub struct AudioEncoder {
    handle: *mut ffi::AvfEncoder,
    _not_sync: PhantomData<std::cell::Cell<()>>,
}

impl AudioEncoder {
    /// Creates an encoder. Use `&WriteOptions::new()` for defaults.
    pub fn new(options: &WriteOptions) -> Result<Self> {
        let (raw, _strings) = options.to_ffi()?;
        // SAFETY: `raw` is initialized and `_strings` keeps the strings it
        // points to alive until after this call.
        let handle = unsafe { ffi::avf_encoder_new(&raw) };
        if handle.is_null() {
            return Err(null_handle_error());
        }
        Ok(Self {
            handle,
            _not_sync: PhantomData,
        })
    }

    /// Writes `samples` (`samples[channel][sample]`) to `path`.
    ///
    /// All channels must have the same length.
    pub fn save(&mut self, path: &str, samples: &[Vec<f32>]) -> Result<()> {
        let path = to_cstring(path)?;
        let (pointers, num_samples) = channel_pointers(samples)?;
        // SAFETY: handle is valid; path and pointers outlive the call; pointers
        // has exactly samples.len() entries, each valid for num_samples floats.
        check(unsafe {
            ffi::avf_encoder_save(
                self.handle,
                path.as_ptr(),
                pointers.as_ptr(),
                pointers.len() as i32,
                num_samples,
            )
        })
    }
}

impl std::fmt::Debug for AudioEncoder {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        // The handle exposes no readable state, so there is nothing to add.
        f.debug_struct("AudioEncoder").finish_non_exhaustive()
    }
}

impl Drop for AudioEncoder {
    fn drop(&mut self) {
        // SAFETY: handle came from avf_encoder_new and is freed exactly once.
        unsafe { ffi::avf_encoder_free(self.handle) };
    }
}

// SAFETY: see the corresponding impl on AudioDecoder.
unsafe impl Send for AudioEncoder {}

/// Writes planar float samples to a file in one call.
///
/// # Examples
///
/// ```no_run
/// use avioflow::{save_audio, WriteOptions};
///
/// # fn main() -> Result<(), avioflow::Error> {
/// let samples = vec![vec![0.0f32; 16000], vec![0.0f32; 16000]];
/// save_audio("out.wav", &samples, &WriteOptions::new()
///     .container_format("wav")
///     .sample_rate(16000))?;
/// # Ok(())
/// # }
/// ```
pub fn save_audio(path: &str, samples: &[Vec<f32>], options: &WriteOptions) -> Result<()> {
    let path = to_cstring(path)?;
    let (raw, _strings) = options.to_ffi()?;
    let (pointers, num_samples) = channel_pointers(samples)?;
    // SAFETY: every pointer passed borrows a local that outlives the call, and
    // the channel array length matches the count reported alongside it.
    check(unsafe {
        ffi::avf_save_audio(
            path.as_ptr(),
            pointers.as_ptr(),
            pointers.len() as i32,
            num_samples,
            &raw,
        )
    })
}
