use super::*;
use crate::bindings::*;

/// Rust version of an `SWSSResult`.
///
/// When an `SWSSResult` is success/`SWSSException_None`, the rust function will return `Ok(..)`.
/// Otherwise, the rust function will return `Err(Exception)`.
pub type Result<T, E = Exception> = std::result::Result<T, E>;

/// "Try" an `SWSSResult`, or a call that produces an `SWSSResult`, and convert it into a rust `Result<T, Exception>`.
macro_rules! swss_try {
    // Convert an SWSSResult to a Result<(), Exception>
    ($result:expr) => {{
        let res = $result;
        if res.exception == $crate::bindings::SWSSException_SWSSException_None {
            Ok(())
        } else {
            Err($crate::types::Exception::take(res))
        }
    }};

    // Call a function that takes an output pointer, then convert its result to a Result<(), Exception>
    ($out_ptr:ident => $result:expr) => {{
        let mut out = ::std::mem::MaybeUninit::uninit();
        let $out_ptr = out.as_mut_ptr();
        let res = $result;
        if res.exception == $crate::bindings::SWSSException_SWSSException_None {
            Ok(out.assume_init())
        } else {
            Err($crate::types::Exception::take(res))
        }
    }};
}
pub(crate) use swss_try;

/// Rust version of the body of a failed `SWSSResult`.
#[derive(Debug, Clone)]
pub struct Exception {
    message: String,
    location: String,
}

impl Exception {
    /// Construct an `Exception` from the details of a failed `SWSSResult`.
    pub(crate) unsafe fn take(res: SWSSResult) -> Self {
        assert_ne!(
            res.exception, SWSSException_SWSSException_None,
            "Exception::take() on non-exception SWSSResult: {res:#?}"
        );
        Self {
            message: CxxString::take(res.message)
                .expect("SWSSResult missing message")
                .to_string_lossy()
                .into_owned(),
            location: CxxString::take(res.location)
                .expect("SWSSResult missing location")
                .to_string_lossy()
                .into_owned(),
        }
    }

    /// Create a new Exception with a custom message (for Rust-side errors).
    /// Automatically captures the caller's file and line number using `#[track_caller]`.
    /// 
    /// # Example
    /// ```
    /// use swss_common::{Exception, Result};
    /// 
    /// fn validate_fd(fd: i32) -> Result<()> {
    ///     if fd < 0 {
    ///         return Err(Exception::new("Invalid file descriptor"));
    ///     }
    ///     Ok(())
    /// }
    /// 
    /// // The exception will automatically include the location where it was created:
    /// let result = validate_fd(-1);
    /// assert!(result.is_err());
    /// let err = result.unwrap_err();
    /// assert_eq!(err.message(), "Invalid file descriptor");
    /// // err.location() will be something like "src/types/exception.rs:70"
    /// ```
    #[track_caller]
    pub fn new(message: impl Into<String>) -> Self {
        let location = std::panic::Location::caller();
        Self {
            message: message.into(),
            location: format!("{}:{}", location.file(), location.line()),
        }
    }

    /// Get an informational string about the error that occurred.
    pub fn message(&self) -> &str {
        &self.message
    }

    /// Get an informational string about the where in the code the error occurred.
    pub fn location(&self) -> &str {
        &self.location
    }
}

impl Display for Exception {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "[{}] {}", self.location, self.message)
    }
}

impl Error for Exception {}
