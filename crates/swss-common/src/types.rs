#[cfg(feature = "async")]
mod async_util;

mod configdbconnector;
mod consumerstatetable;
mod cxxstring;
mod dbconnector;
mod events;
mod exception;
mod logger;
mod producerstatetable;
mod sonicv2connector;
mod subscriberstatetable;
mod table;
mod zmqclient;
mod zmqconsumerstatetable;
mod zmqproducerstatetable;
mod zmqserver;

pub use configdbconnector::{ConfigDBConnector, BorrowedDbConnector};
pub use consumerstatetable::ConsumerStateTable;
pub use cxxstring::{CxxStr, CxxString};
pub use dbconnector::{DbConnectionInfo, DbConnector};
pub use events::EventPublisher;
pub use exception::{Exception, Result};
pub use logger::{link_to_swsscommon_logger, log_level, log_output, LoggerConfigChangeHandler};
pub use producerstatetable::ProducerStateTable;
pub use sonicv2connector::SonicV2Connector;
pub use subscriberstatetable::SubscriberStateTable;
pub use table::Table;
pub use zmqclient::ZmqClient;
pub use zmqconsumerstatetable::ZmqConsumerStateTable;
pub use zmqproducerstatetable::ZmqProducerStateTable;
pub use zmqserver::ZmqServer;

pub(crate) use exception::swss_try;

use crate::bindings::*;
use cxxstring::RawMutableSWSSString;
use serde::{Deserialize, Serialize};
use std::{
    any::Any,
    collections::HashMap,
    error::Error,
    ffi::{CStr, CString},
    fmt::Display,
    slice,
    str::FromStr,
};

pub(crate) fn cstr(s: impl AsRef<[u8]>) -> Result<CString> {
    CString::new(s.as_ref())
        .map_err(|e| Exception::new(format!("String contains null byte at position {}", e.nul_position())))
}

/// Take a malloc'd c string and convert it to a native String
/// If any string fails to convert, returns an error and frees all allocated memory.
pub(crate) unsafe fn take_cstr(p: *const libc::c_char) -> Result<String> {
    let cstr = CStr::from_ptr(p);
    // Convert to Rust String, capturing UTF-8 conversion errors.
    let result: Result<String> = match cstr.to_str() {
        Ok(s) => Ok(s.to_string()),
        Err(_) => Err(Exception::new("C string being converted to Rust String contains invalid UTF-8".to_string())),
    };

    // Free the original C string in all cases to avoid leaks.
    libc::free(p as *mut libc::c_void);

    result
}

/// Rust version of the return type from `swss::Select::select`.
///
/// This enum does not include the `swss::Select::ERROR` because errors are handled via a different
/// mechanism in this library.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum SelectResult {
    /// Data is now available.
    /// (`swss::Select::OBJECT`)
    Data,
    /// Waiting was interrupted by a signal.
    /// (`swss::Select::SIGNALINT`)
    Signal,
    /// Timed out.
    /// (`swss::Select::TIMEOUT`)
    Timeout,
}

impl SelectResult {
    pub(crate) fn from_raw(raw: SWSSSelectResult) -> Self {
        if raw == SWSSSelectResult_SWSSSelectResult_DATA {
            SelectResult::Data
        } else if raw == SWSSSelectResult_SWSSSelectResult_SIGNAL {
            SelectResult::Signal
        } else if raw == SWSSSelectResult_SWSSSelectResult_TIMEOUT {
            SelectResult::Timeout
        } else {
            unreachable!("Invalid SWSSSelectResult: {raw}");
        }
    }
}

/// Type of the `operation` field in [KeyOpFieldValues].
///
/// In swsscommon, this is represented as a string of `"SET"` or `"DEL"`.
/// This type can be constructed similarly - `let op: KeyOperation = "SET".parse().unwrap()`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
pub enum KeyOperation {
    Set,
    Del,
}

impl KeyOperation {
    pub(crate) fn as_raw(self) -> SWSSKeyOperation {
        match self {
            KeyOperation::Set => SWSSKeyOperation_SWSSKeyOperation_SET,
            KeyOperation::Del => SWSSKeyOperation_SWSSKeyOperation_DEL,
        }
    }

    pub(crate) fn from_raw(raw: SWSSKeyOperation) -> Self {
        if raw == SWSSKeyOperation_SWSSKeyOperation_SET {
            KeyOperation::Set
        } else if raw == SWSSKeyOperation_SWSSKeyOperation_DEL {
            KeyOperation::Del
        } else {
            unreachable!("Invalid SWSSKeyOperation: {raw}");
        }
    }
}

impl FromStr for KeyOperation {
    type Err = InvalidKeyOperationString;

    /// Create a KeyOperation from `"SET"` or `"DEL"` (case insensitive).
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let Ok(mut bytes): Result<[u8; 3], _> = s.as_bytes().try_into() else {
            return Err(InvalidKeyOperationString(s.to_string()));
        };
        bytes.make_ascii_uppercase();
        match &bytes {
            b"SET" => Ok(Self::Set),
            b"DEL" => Ok(Self::Del),
            _ => Err(InvalidKeyOperationString(s.to_string())),
        }
    }
}

/// Error type indicating that a `KeyOperation` string was neither `"SET"` nor `"DEL"`.
#[derive(Debug)]
pub struct InvalidKeyOperationString(String);

impl Display for InvalidKeyOperationString {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, r#"A KeyOperation String must be "SET" or "DEL", but was {}"#, self.0)
    }
}

impl Error for InvalidKeyOperationString {}

/// Rust version of `vector<swss::FieldValueTuple>`.
pub type FieldValues = HashMap<String, CxxString>;

/// Rust version of `swss::KeyOpFieldsValuesTuple`.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct KeyOpFieldValues {
    pub key: String,
    pub operation: KeyOperation,
    pub field_values: FieldValues,
}

impl KeyOpFieldValues {
    pub fn set<K, I, F, V>(key: K, fvs: I) -> Self
    where
        K: Into<String>,
        I: IntoIterator<Item = (F, V)>,
        F: Into<String>,
        V: Into<CxxString>,
    {
        Self {
            key: key.into(),
            operation: KeyOperation::Set,
            field_values: fvs.into_iter().map(|(f, v)| (f.into(), v.into())).collect(),
        }
    }

    pub fn del<K>(key: K) -> Self
    where
        K: Into<String>,
    {
        Self {
            key: key.into(),
            operation: KeyOperation::Del,
            field_values: HashMap::new(),
        }
    }
}

/// Intended for testing, ordered by key.
impl PartialOrd for KeyOpFieldValues {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

/// Intended for testing, ordered by key.
impl Ord for KeyOpFieldValues {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.key.cmp(&other.key)
    }
}

/// Takes ownership of an `SWSSFieldValueArray` and turns it into a native representation.
/// If any entry fails to convert, returns an error and frees all allocated memory.
pub(crate) unsafe fn take_field_value_array(arr: SWSSFieldValueArray) -> Result<FieldValues> {
    let mut out = HashMap::with_capacity(arr.len as usize);
    let mut err: Option<Exception> = None;

    if !arr.data.is_null() {
        let entries = slice::from_raw_parts(arr.data, arr.len as usize);
        for fv in entries {
            // Try to convert the field name. If it fails, remember the error but
            // continue and still attempt to take the value (to free any owned memory).
            match take_cstr(fv.field) {
                Ok(field) => {
                    match CxxString::take(fv.value) {
                        Some(value) => {
                            out.insert(field, value);
                        }
                        None => {
                            if err.is_none() {
                                err = Some(Exception::new("CxxString::take returned null".to_string()));
                            }
                        }
                    }
                }
                Err(e) => {
                    // record first error
                    if err.is_none() {
                        err = Some(e);
                    }
                    // still try to take value to free it if present
                    let _ = CxxString::take(fv.value);
                }
            }
        }
        SWSSFieldValueArray_free(arr);
    }

    match err {
        Some(e) => Err(e),
        None => Ok(out),
    }
}

/// Takes ownership of an `SWSSKeyOpFieldValuesArray` and turns it into a native representation.
/// If any entry fails to convert, returns an error and frees all allocated memory.
pub(crate) unsafe fn take_key_op_field_values_array(kfvs: SWSSKeyOpFieldValuesArray) -> Result<Vec<KeyOpFieldValues>> {
    let mut out = Vec::with_capacity(kfvs.len as usize);
    let mut err: Option<Exception> = None;

    if !kfvs.data.is_null() {
        unsafe {
            let entries = slice::from_raw_parts(kfvs.data, kfvs.len as usize);
            for kfv in entries {
                // attempt to get key
                let key_res = take_cstr(kfv.key);
                let operation = KeyOperation::from_raw(kfv.operation);

                // attempt to get field_values
                let field_values_res = take_field_value_array(kfv.fieldValues);

                match (key_res, field_values_res) {
                    (Ok(key), Ok(field_values)) => {
                        out.push(KeyOpFieldValues { key, operation, field_values });
                    }
                    (kres, fvres) => {
                        // record first error, if any
                        if err.is_none() {
                            if let Err(e) = kres { err = Some(e); }
                            else if let Err(e) = fvres { err = Some(e); }
                        }
                        // continue to next entry to ensure C resources are freed
                    }
                }
            }
            SWSSKeyOpFieldValuesArray_free(kfvs);
        };
    }

    match err {
        Some(e) => Err(e),
        None => Ok(out),
    }
}

/// Takes ownership of an `SWSSStringArray` and turns it into a native representation.
/// If any string fails to convert, returns an error and frees all allocated memory.
pub(crate) unsafe fn take_string_array(arr: SWSSStringArray) -> Result<Vec<String>> {
    if !arr.data.is_null() {
        let entries = slice::from_raw_parts(arr.data, arr.len as usize);
        let mut out = Vec::with_capacity(entries.len());
        let mut err: Option<Exception> = None;

        for &s in entries {
            match take_cstr(s) {
                Ok(string) => out.push(string),
                Err(e) => {
                    if err.is_none() {
                        err = Some(e);
                    }
                }
            }
        }

        // Free the array before returning error or success.
        SWSSStringArray_free(arr);

        match err {
            Some(e) => Err(e),
            None => Ok(out),
        }
    } else {
        SWSSStringArray_free(arr);
        Ok(Vec::new())
    }
}


pub(crate) fn make_field_value_array<I, F, V>(fvs: I) -> Result<(SWSSFieldValueArray, KeepAlive)>
where
    I: IntoIterator<Item = (F, V)>,
    F: AsRef<[u8]>,
    V: Into<CxxString>,
{
    let mut k = KeepAlive::default();
    let mut data = Vec::new();

    for (field, value) in fvs {
        let field = cstr(field)?;
        let value_cxxstring: CxxString = value.into();
        let value_rawswssstring: RawMutableSWSSString = value_cxxstring.into_raw();
        data.push(SWSSFieldValueTuple {
            field: field.as_ptr(),
            value: value_rawswssstring.as_raw(),
        });
        k.keep((field, value_rawswssstring));
    }

    let arr = SWSSFieldValueArray {
        data: data.as_mut_ptr(),
        len: data.len().try_into().map_err(|_| Exception::new(
            format!("field value array length {} exceeds maximum for target type", data.len())
        ))?,
    };
    k.keep(data);

    Ok((arr, k))
}

pub(crate) fn make_key_op_field_values_array<I>(kfvs: I) -> Result<(SWSSKeyOpFieldValuesArray, KeepAlive)>
where
    I: IntoIterator<Item = KeyOpFieldValues>,
{
    let mut k = KeepAlive::default();
    let mut data = Vec::new();

    for kfv in kfvs {
        let key = cstr(kfv.key)?;
        let operation = kfv.operation.as_raw();
        let (field_values, arr_k) = make_field_value_array(kfv.field_values)?;
        data.push(SWSSKeyOpFieldValues {
            key: key.as_ptr(),
            operation,
            fieldValues: field_values,
        });
        k.keep(Box::new((key, arr_k)))
    }

    let arr = SWSSKeyOpFieldValuesArray {
        data: data.as_mut_ptr(),
        len: data.len().try_into().map_err(|_| Exception::new(
            format!("key-op field values array length {} exceeds maximum for target type", data.len())
        ))?,
    };
    k.keep(Box::new(data));

    Ok((arr, k))
}

/// Helper struct to keep rust-owned data alive while it is in use by C++
#[derive(Default)]
pub(crate) struct KeepAlive(Vec<Box<dyn Any>>);

impl KeepAlive {
    fn keep<T: Any>(&mut self, t: T) {
        self.0.push(Box::new(t))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    // Helper function to create a C string pointer from a Rust string.
    fn make_c_string(s: &str) -> *const libc::c_char {
        let c_str = CString::new(s).expect("CString::new failed");
        let ptr = c_str.as_ptr();
        // Leak the CString so we can pass ownership to take_cstr.
        std::mem::forget(c_str);
        ptr
    }

    #[test]
    fn test_take_cstr_valid_utf8() {
        let c_ptr = make_c_string("Hello, World!");
        let result = unsafe { take_cstr(c_ptr) };

        assert!(result.is_ok());
        assert_eq!(result.unwrap(), "Hello, World!");
    }

    #[test]
    fn test_take_cstr_invalid_utf8() {
        // Create an invalid UTF-8 sequence by manually crafting a C string
        let invalid_bytes: Vec<u8> = vec![0xFF, 0xFE, 0x00];
        let ptr = unsafe {
            let buf = libc::malloc(invalid_bytes.len() + 1) as *mut u8;
            if buf.is_null() {
                panic!("malloc failed");
            }
            std::ptr::copy_nonoverlapping(invalid_bytes.as_ptr(), buf, invalid_bytes.len());
            *buf.add(invalid_bytes.len()) = 0; // null terminator
            buf as *const libc::c_char
        };

        let result = unsafe { take_cstr(ptr) };

        // Should be an error due to invalid UTF-8
        assert!(result.is_err());

        // Verify the error message indicates UTF-8 issue
        let err = result.unwrap_err();
        assert!(err.to_string().contains("invalid UTF-8"));

    }

    #[test]
    fn test_take_string_array_empty() {
        let arr = SWSSStringArray {
            data: std::ptr::null_mut(),
            len: 0,
        };

        let result = unsafe { take_string_array(arr) };

        assert!(result.is_ok());
        assert_eq!(result.unwrap().len(), 0);
    }

    #[test]
    fn test_take_string_array_invalid_utf8() {
        // Create an invalid UTF-8 sequence by manually crafting a C string
        let invalid_bytes: Vec<u8> = vec![0xFF, 0xFE, 0x00];
        let invalid_ptr = unsafe {
            let buf = libc::malloc(invalid_bytes.len() + 1) as *mut u8;
            if buf.is_null() {
                panic!("malloc failed");
            }
            std::ptr::copy_nonoverlapping(invalid_bytes.as_ptr(), buf, invalid_bytes.len());
            *buf.add(invalid_bytes.len()) = 0; // null terminator
            buf as *const libc::c_char
        };

        // Create a valid UTF-8 string
        let valid_ptr = make_c_string("valid string");

        // Allocate the array pointer with malloc to hold two strings
        let arr_data = unsafe {
            let data_ptr = libc::malloc(std::mem::size_of::<*const libc::c_char>() * 2) as *mut *const libc::c_char;
            if data_ptr.is_null() {
                panic!("malloc failed");
            }
            *data_ptr = invalid_ptr;
            *data_ptr.add(1) = valid_ptr;
            data_ptr
        };

        let arr = SWSSStringArray {
            data: arr_data,
            len: 2,
        };

        let result = unsafe { take_string_array(arr) };

        // Should be an error due to invalid UTF-8 in the first string
        assert!(result.is_err());

        // Verify the error message indicates UTF-8 issue
        let err = result.unwrap_err();
        assert!(err.to_string().contains("invalid UTF-8"));
    }
}

