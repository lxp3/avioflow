//! Error type and the helpers that turn C status codes into `Result`s.

use std::ffi::{CStr, CString, NulError};
use std::fmt;
use std::os::raw::c_int;

use crate::ffi;

/// The kind of failure, derived from the C ABI status code.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorKind {
    /// An argument was rejected before any work happened.
    InvalidArgument,
    /// The operation failed while running (unreadable file, unsupported codec).
    Runtime,
    /// A path or format string contained an interior NUL byte, so it could not
    /// be passed to C. This never reaches the native layer.
    InvalidString,
    /// The native layer failed without classifying the cause.
    Unknown,
}

/// An error returned by an avioflow operation.
#[derive(Debug, Clone)]
pub struct Error {
    kind: ErrorKind,
    message: String,
}

impl Error {
    pub(crate) fn new(kind: ErrorKind, message: impl Into<String>) -> Self {
        Self {
            kind,
            message: message.into(),
        }
    }

    /// Classification of the failure.
    pub fn kind(&self) -> ErrorKind {
        self.kind
    }

    /// Message from the native layer, or from the binding for `InvalidString`.
    pub fn message(&self) -> &str {
        &self.message
    }

    /// Builds an error from the status code plus the thread-local message the
    /// native layer recorded for the call that just failed.
    fn from_status(status: c_int) -> Self {
        let kind = match status {
            -1 => ErrorKind::InvalidArgument,
            -2 => ErrorKind::Runtime,
            _ => ErrorKind::Unknown,
        };

        // SAFETY: avf_last_error() always returns a valid NUL-terminated
        // pointer to thread-local storage, never null. It is read immediately,
        // before any other call on this thread can overwrite it.
        let message = unsafe {
            let raw = ffi::avf_last_error();
            if raw.is_null() {
                String::new()
            } else {
                CStr::from_ptr(raw).to_string_lossy().into_owned()
            }
        };

        let message = if message.is_empty() {
            format!("avioflow call failed with status {status}")
        } else {
            message
        };

        Self::new(kind, message)
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.message)
    }
}

impl std::error::Error for Error {}

impl From<NulError> for Error {
    fn from(error: NulError) -> Self {
        Self::new(ErrorKind::InvalidString, error.to_string())
    }
}

/// Result alias used throughout the crate.
pub type Result<T> = std::result::Result<T, Error>;

/// Converts a C status code into a `Result`.
pub(crate) fn check(status: c_int) -> Result<()> {
    if status == ffi::AVF_OK {
        Ok(())
    } else {
        Err(Error::from_status(status))
    }
}

/// Builds the error for a constructor that signalled failure by returning null.
///
/// Such functions have no status code of their own, so the classification is
/// read back from the thread-local the native layer recorded.
pub(crate) fn null_handle_error() -> Error {
    // SAFETY: reads a thread-local int; always safe to call.
    let status = unsafe { ffi::avf_last_error_code() };
    // A null handle means failure, so never report success even if the code was
    // somehow not set.
    let status = if status == ffi::AVF_OK { -2 } else { status };
    Error::from_status(status)
}

/// Converts a Rust string for the C ABI, rejecting interior NUL bytes.
pub(crate) fn to_cstring(value: &str) -> Result<CString> {
    Ok(CString::new(value)?)
}
