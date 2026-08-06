//! Audio decoding.

use std::marker::PhantomData;

use crate::error::{check, null_handle_error, to_cstring, Result};
use crate::ffi;
use crate::metadata::Metadata;
use crate::options::StreamOptions;
use crate::samples::OwnedSamples;

/// A borrowed view of one decoded frame.
///
/// The data points into decoder-owned buffers and is invalidated by the next
/// decode call, which the borrow of the decoder enforces at compile time.
pub struct Frame<'a> {
    channels: Vec<&'a [f32]>,
}

impl<'a> Frame<'a> {
    /// Number of channels in this frame.
    pub fn num_channels(&self) -> usize {
        self.channels.len()
    }

    /// Number of samples per channel.
    pub fn num_samples(&self) -> usize {
        self.channels.first().map_or(0, |c| c.len())
    }

    /// Samples for `channel`, or `None` when out of range.
    pub fn channel(&self, channel: usize) -> Option<&'a [f32]> {
        self.channels.get(channel).copied()
    }

    /// All channels as planar slices.
    pub fn channels(&self) -> &[&'a [f32]] {
        &self.channels
    }

    /// Copies the frame into owned vectors.
    pub fn to_vecs(&self) -> Vec<Vec<f32>> {
        self.channels.iter().map(|c| c.to_vec()).collect()
    }
}

impl std::fmt::Debug for Frame<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Frame")
            .field("num_channels", &self.num_channels())
            .field("num_samples", &self.num_samples())
            .finish()
    }
}

/// Decodes audio from a file, URL, memory buffer, device, or a pushed byte
/// stream.
///
/// # Examples
///
/// Decode a whole file, resampled to 16 kHz:
///
/// ```no_run
/// use avioflow::{AudioDecoder, StreamOptions};
///
/// # fn main() -> Result<(), avioflow::Error> {
/// let mut decoder = AudioDecoder::new(&StreamOptions::new().output_sample_rate(16000))?;
/// let metadata = decoder.load_file("audio.mp3")?;
/// let samples = decoder.get_samples()?;
/// println!("{} channels at {} Hz", samples.len(), metadata.sample_rate);
/// # Ok(())
/// # }
/// ```
///
/// Decode only seconds 10.3 through 20.3:
///
/// ```no_run
/// # use avioflow::{AudioDecoder, StreamOptions};
/// # fn main() -> Result<(), avioflow::Error> {
/// let mut decoder = AudioDecoder::new(&StreamOptions::new())?;
/// decoder.load_file("audio.mp3")?;
/// let samples = decoder.get_samples_range(10.3, Some(20.3))?;
/// # Ok(())
/// # }
/// ```
pub struct AudioDecoder {
    handle: *mut ffi::AvfDecoder,
    // The handle owns FFmpeg state that is not safe to touch from two threads
    // at once, so the decoder is Send but deliberately not Sync.
    _not_sync: PhantomData<std::cell::Cell<()>>,
}

impl AudioDecoder {
    /// Creates a decoder. Use `&StreamOptions::new()` for defaults.
    pub fn new(options: &StreamOptions) -> Result<Self> {
        let (raw, _format) = options.to_ffi()?;
        // SAFETY: `raw` is a valid initialized struct, and `_format` keeps the
        // string it points to alive until after this call returns.
        let handle = unsafe { ffi::avf_decoder_new(&raw) };
        if handle.is_null() {
            return Err(null_handle_error());
        }
        Ok(Self {
            handle,
            _not_sync: PhantomData,
        })
    }

    /// Opens a file path, URL, or device identifier.
    pub fn load_file(&mut self, source: &str) -> Result<Metadata> {
        let source = to_cstring(source)?;
        let mut raw = ffi::AvfMetadata::default();
        // SAFETY: handle is valid for the lifetime of self; source outlives the
        // call; raw is a live local the callee only writes to.
        check(unsafe { ffi::avf_decoder_load_file(self.handle, source.as_ptr(), &mut raw) })?;
        Ok(Metadata::from_ffi(&raw))
    }

    /// Opens complete audio file bytes held in memory.
    pub fn load_buffer(&mut self, data: &[u8]) -> Result<Metadata> {
        let mut raw = ffi::AvfMetadata::default();
        // SAFETY: data.as_ptr()/len() describe a valid slice borrowed for the
        // duration of the call.
        check(unsafe {
            ffi::avf_decoder_load_buffer(self.handle, data.as_ptr(), data.len(), &mut raw)
        })?;
        Ok(Metadata::from_ffi(&raw))
    }

    /// Pushes encoded bytes for streaming decode.
    ///
    /// Requires [`StreamOptions::input_format`] to have been set.
    pub fn feed(&mut self, data: &[u8]) -> Result<()> {
        // SAFETY: as in load_buffer.
        check(unsafe { ffi::avf_decoder_feed(self.handle, data.as_ptr(), data.len()) })
    }

    /// Marks streaming input complete, so remaining buffered frames can drain.
    pub fn flush(&mut self) -> Result<()> {
        // SAFETY: handle is valid for the lifetime of self.
        check(unsafe { ffi::avf_decoder_flush(self.handle) })
    }

    /// Decodes the next frame without copying.
    ///
    /// Returns `None` at end of stream, or when streaming input needs more data.
    /// The returned frame borrows `self`, so the next decode call cannot happen
    /// while it is alive.
    pub fn get_frame(&mut self) -> Result<Option<Frame<'_>>> {
        let mut raw = ffi::AvfFrame::default();
        // SAFETY: handle is valid; raw is a live local the callee only writes to.
        check(unsafe { ffi::avf_decoder_get_frame(self.handle, &mut raw) })?;

        if raw.data.is_null() || raw.num_samples <= 0 || raw.num_channels <= 0 {
            return Ok(None);
        }

        // SAFETY: on a non-empty frame the C API guarantees `data` points to
        // num_channels pointers, each to num_samples floats. The returned
        // Frame borrows self, so these slices cannot outlive the next decode
        // call that would invalidate them.
        let channels = unsafe {
            let pointers = std::slice::from_raw_parts(raw.data, raw.num_channels as usize);
            pointers
                .iter()
                .map(|&p| std::slice::from_raw_parts(p, raw.num_samples as usize))
                .collect()
        };

        Ok(Some(Frame { channels }))
    }

    /// Decodes all remaining samples as `samples[channel][sample]`.
    pub fn get_samples(&mut self) -> Result<Vec<Vec<f32>>> {
        self.get_samples_range(0.0, None)
    }

    /// Decodes the half-open range `[start_seconds, stop_seconds)`.
    ///
    /// Pass `None` for `stop_seconds` to decode through to the end. Range
    /// decoding requires offline mode (a loaded file or buffer); in stream mode
    /// only `(0.0, None)` is valid.
    pub fn get_samples_range(
        &mut self,
        start_seconds: f64,
        stop_seconds: Option<f64>,
    ) -> Result<Vec<Vec<f32>>> {
        let owned = OwnedSamples::from_call(|out| {
            // SAFETY: handle is valid; out points to a live local pointer.
            unsafe {
                ffi::avf_decoder_get_samples(
                    self.handle,
                    start_seconds,
                    stop_seconds.unwrap_or(0.0),
                    i32::from(stop_seconds.is_some()),
                    out,
                )
            }
        })?;
        Ok(owned.to_vecs())
    }

    /// Whether the stream is exhausted.
    pub fn is_finished(&self) -> Result<bool> {
        let mut finished = 0;
        // SAFETY: handle is valid; finished is a live local.
        check(unsafe { ffi::avf_decoder_is_finished(self.handle, &mut finished) })?;
        Ok(finished != 0)
    }

    /// Current stream metadata.
    pub fn metadata(&self) -> Result<Metadata> {
        let mut raw = ffi::AvfMetadata::default();
        // SAFETY: handle is valid; raw is a live local.
        check(unsafe { ffi::avf_decoder_get_metadata(self.handle, &mut raw) })?;
        Ok(Metadata::from_ffi(&raw))
    }
}

impl std::fmt::Debug for AudioDecoder {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("AudioDecoder")
            .field("metadata", &self.metadata().ok())
            .field("finished", &self.is_finished().ok())
            .finish()
    }
}

impl Drop for AudioDecoder {
    fn drop(&mut self) {
        // SAFETY: handle came from avf_decoder_new, is owned by self, and is
        // freed exactly once.
        unsafe { ffi::avf_decoder_free(self.handle) };
    }
}

// SAFETY: the handle is owned exclusively by this value and the underlying
// decoder holds no thread-affine state, so moving it between threads is sound.
// Sync is deliberately not implemented: concurrent method calls on one decoder
// are not supported by the C++ core.
unsafe impl Send for AudioDecoder {}
