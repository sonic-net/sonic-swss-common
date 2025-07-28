use crate::bindings::*;
use serde::{Deserialize, Serialize};
use std::{
    borrow::{Borrow, Cow},
    fmt::Debug,
    hash::Hash,
    ops::Deref,
    ptr::NonNull,
    slice,
    str::Utf8Error,
};

/// A C++ `std::string` that can be moved around and accessed from Rust.
#[repr(transparent)]
#[derive(Eq)]
pub struct CxxString {
    ptr: NonNull<SWSSStringOpaque>,
}

impl CxxString {
    /// Construct a CxxString from an SWSSString.
    /// The `CxxString` is now responsible for freeing the string, so it should not be freed manually.
    /// Returns `None` if `s` is null.
    pub(crate) unsafe fn take(s: SWSSString) -> Option<CxxString> {
        NonNull::new(s).map(|ptr| CxxString { ptr })
    }

    /// Convert `self` into a wrapper type which provides safe mutable access to the underlying `std::string`.
    pub(crate) fn into_raw(self) -> RawMutableSWSSString {
        RawMutableSWSSString(self)
    }

    /// Copies the given data into a new C++ string.
    pub fn new(data: impl AsRef<[u8]>) -> CxxString {
        unsafe {
            let ptr = data.as_ref().as_ptr() as *const libc::c_char;
            let len = data.as_ref().len().try_into().unwrap();
            CxxString::take(SWSSString_new(ptr, len)).unwrap()
        }
    }

    /// Borrows a `CxxStr` from this string.
    ///
    /// Like `String::as_str`, this method is unnecessary where deref coercion can be used.
    pub fn as_cxx_str(&self) -> &CxxStr {
        self
    }
}

impl Serialize for CxxString {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        self.as_cxx_str().serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for CxxString {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        struct CxxStringVisitor;

        impl serde::de::Visitor<'_> for CxxStringVisitor {
            type Value = CxxString;

            fn expecting(&self, formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
                formatter.write_str("string or bytes")
            }

            fn visit_bytes<E>(self, v: &[u8]) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                Ok(CxxString::new(v))
            }

            fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                Ok(CxxString::new(v))
            }
        }

        deserializer.deserialize_bytes(CxxStringVisitor)
    }
}

/// Wrapper type around `CxxString` which disables access to methods that borrow the underlying
/// data, like `.deref()`. This prevents code from creating an `SWSSString` and `SWSSStrRef` at the
/// same time, which would cause a data race in multi-threaded contexts.
///
/// A similar struct could be made which would allow access via just an `&mut CxxString`, but any
/// function taking `SWSSString` will destroy the underlying data anyway, so we might as well just
/// drop it when we're done.
///
/// This newtype needs to exist (as opposed to into_raw returning the raw pointer) so that the
/// string is not dropped immediately.
pub(crate) struct RawMutableSWSSString(CxxString);

impl RawMutableSWSSString {
    pub(crate) fn as_raw(&self) -> SWSSString {
        self.0.ptr.as_ptr()
    }
}

impl<T: AsRef<[u8]>> From<T> for CxxString {
    fn from(bytes: T) -> Self {
        CxxString::new(bytes.as_ref())
    }
}

impl Drop for CxxString {
    fn drop(&mut self) {
        unsafe { SWSSString_free(self.ptr.as_ptr()) }
    }
}

/// This calls [CxxStr::to_string_lossy] which may clone the underlying data (i.e. may be expensive).
impl Debug for CxxString {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        self.deref().fmt(f)
    }
}

impl Clone for CxxString {
    fn clone(&self) -> Self {
        CxxString::new(self.as_bytes())
    }
}

/// SAFETY: A C++ string has no thread related state.
unsafe impl Send for CxxString {}

/// SAFETY: A `CxxString` can only be mutated through `.into_raw()` which takes ownership of `self`.
/// Thus, normal Rust borrow checking rules will prevent data races.
unsafe impl Sync for CxxString {}

impl Deref for CxxString {
    type Target = CxxStr;

    fn deref(&self) -> &Self::Target {
        // SAFETY: CxxString and CxxStr are both repr(transparent) and identical in alignment &
        // size, and the C API guarantees that SWSSString can always be cast into SWSSStrRef
        unsafe { std::mem::transmute(self) }
    }
}

impl PartialOrd for CxxString {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for CxxString {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.deref().cmp(other)
    }
}

impl PartialEq for CxxString {
    fn eq(&self, other: &Self) -> bool {
        self.deref().eq(other)
    }
}

impl PartialEq<CxxStr> for CxxString {
    fn eq(&self, other: &CxxStr) -> bool {
        self.deref().eq(other)
    }
}

impl PartialEq<str> for CxxString {
    fn eq(&self, other: &str) -> bool {
        self.deref().eq(other)
    }
}

impl PartialEq<&str> for CxxString {
    fn eq(&self, other: &&str) -> bool {
        self.deref().eq(other)
    }
}

impl PartialEq<String> for CxxString {
    fn eq(&self, other: &String) -> bool {
        self.eq(other.as_str())
    }
}

impl Hash for CxxString {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.deref().hash(state);
    }
}

impl Borrow<CxxStr> for CxxString {
    fn borrow(&self) -> &CxxStr {
        self.deref()
    }
}

/// Equivalent of a C++ `const std::string&`, which can be borrowed from a [`CxxString`].
///
/// `CxxStr` has the same conceptual relationship with `CxxString` as a Rust `&str` does with `String`.
#[repr(transparent)]
#[derive(Eq)]
pub struct CxxStr {
    ptr: NonNull<SWSSStrRefOpaque>,
}

impl CxxStr {
    /// This is safe because the C api guarantees that `SWSSStrRef` is read-only. On `CxxString`, this would not be safe.
    pub(crate) fn as_raw(&self) -> SWSSStrRef {
        self.ptr.as_ptr()
    }

    /// Length of the string, not including a null terminator.
    pub fn len(&self) -> usize {
        unsafe { SWSSStrRef_length(self.as_raw()).try_into().unwrap() }
    }

    /// Returns `true` if `self` has a length of zero bytes.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// The underlying bytes of the string, not including a null terminator.
    pub fn as_bytes(&self) -> &[u8] {
        unsafe {
            let data = SWSSStrRef_c_str(self.as_raw());
            slice::from_raw_parts(data as *const u8, self.len())
        }
    }

    /// Tries to convert the C++ string to a Rust `&str` without copying. This can only be done if
    /// the string contains valid UTF-8. See [std::str::from_utf8].
    pub fn to_str(&self) -> Result<&str, Utf8Error> {
        std::str::from_utf8(self.as_bytes())
    }

    /// Converts the C++ string to a Rust `&str` or `String`. If the string is valid UTF-8, the
    /// result is a `&str` pointing to the original data. Otherwise, the result is a `String` with
    /// a copy of the data, but with invalid UTF-8 replaced. See [String::from_utf8_lossy].
    pub fn to_string_lossy(&self) -> Cow<'_, str> {
        String::from_utf8_lossy(self.as_bytes())
    }
}

impl ToOwned for CxxStr {
    type Owned = CxxString;

    fn to_owned(&self) -> Self::Owned {
        CxxString::new(self.as_bytes())
    }
}

impl Debug for CxxStr {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "\"{}\"", self.to_string_lossy())
    }
}

impl PartialOrd for CxxStr {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for CxxStr {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.as_bytes().cmp(other.as_bytes())
    }
}

impl Hash for CxxStr {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        self.as_bytes().hash(state);
    }
}

impl PartialEq for CxxStr {
    fn eq(&self, other: &Self) -> bool {
        self.as_bytes().eq(other.as_bytes())
    }
}

impl PartialEq<CxxString> for CxxStr {
    fn eq(&self, other: &CxxString) -> bool {
        self.as_bytes().eq(other.as_bytes())
    }
}

impl PartialEq<str> for CxxStr {
    fn eq(&self, other: &str) -> bool {
        self.as_bytes().eq(other.as_bytes())
    }
}

impl PartialEq<&str> for CxxStr {
    fn eq(&self, other: &&str) -> bool {
        self.eq(*other)
    }
}

impl PartialEq<String> for CxxStr {
    fn eq(&self, other: &String) -> bool {
        self.eq(other.as_str())
    }
}

/// SAFETY: `CxxStr` can never be obtained by value, only as a reference by dereferencing a
/// `CxxString`. Thus, the same logic as implementing `Sync` for `CxxString` applies. Mutation
/// is protected by `CxxString::into_raw` requiring an owned `self`. By the same logic, `Send` is
/// unnecessary to implement (you can never obtain a value to send).
unsafe impl Sync for CxxStr {}

impl Serialize for CxxStr {
    fn serialize<S: serde::Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        match self.to_str() {
            Ok(s) => serializer.serialize_str(s),
            Err(_) => serializer.serialize_bytes(self.as_bytes()),
        }
    }
}
