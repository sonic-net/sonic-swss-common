#[cfg(feature = "async")]
mod async_util;

mod consumerstatetable;
mod cxxstring;
mod dbconnector;
mod exception;
mod logger;
mod producerstatetable;
mod subscriberstatetable;
mod table;
mod zmqclient;
mod zmqconsumerstatetable;
mod zmqproducerstatetable;
mod zmqserver;

pub use consumerstatetable::ConsumerStateTable;
pub use cxxstring::{CxxStr, CxxString};
pub use dbconnector::{DbConnectionInfo, DbConnector};
pub use exception::{Exception, Result};
pub use logger::{link_to_swsscommon_logger, log_level, log_output, LoggerConfigChangeHandler};
pub use producerstatetable::ProducerStateTable;
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

pub(crate) fn cstr(s: impl AsRef<[u8]>) -> CString {
    CString::new(s.as_ref()).expect("Bytes being converted to a C string already contains a null byte")
}

/// Take a malloc'd c string and convert it to a native String
pub(crate) unsafe fn take_cstr(p: *const libc::c_char) -> String {
    let s = CStr::from_ptr(p)
        .to_str()
        .expect("C string being converted to Rust String contains invalid UTF-8")
        .to_string();
    libc::free(p as *mut libc::c_void);
    s
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
pub(crate) unsafe fn take_field_value_array(arr: SWSSFieldValueArray) -> FieldValues {
    let mut out = HashMap::with_capacity(arr.len as usize);
    if !arr.data.is_null() {
        let entries = slice::from_raw_parts(arr.data, arr.len as usize);
        for fv in entries {
            let field = take_cstr(fv.field);
            let value = CxxString::take(fv.value).unwrap();
            out.insert(field, value);
        }
        SWSSFieldValueArray_free(arr);
    }
    out
}

/// Takes ownership of an `SWSSKeyOpFieldValuesArray` and turns it into a native representation.
pub(crate) unsafe fn take_key_op_field_values_array(kfvs: SWSSKeyOpFieldValuesArray) -> Vec<KeyOpFieldValues> {
    let mut out = Vec::with_capacity(kfvs.len as usize);
    if !kfvs.data.is_null() {
        unsafe {
            let entries = slice::from_raw_parts(kfvs.data, kfvs.len as usize);
            for kfv in entries {
                let key = take_cstr(kfv.key);
                let operation = KeyOperation::from_raw(kfv.operation);
                let field_values = take_field_value_array(kfv.fieldValues);
                out.push(KeyOpFieldValues {
                    key,
                    operation,
                    field_values,
                });
            }
            SWSSKeyOpFieldValuesArray_free(kfvs);
        };
    }
    out
}

/// Takes ownership of an `SWSSStringArray` and turns it into a native representation.
pub(crate) unsafe fn take_string_array(arr: SWSSStringArray) -> Vec<String> {
    let out = if !arr.data.is_null() {
        let entries = slice::from_raw_parts(arr.data, arr.len as usize);
        Vec::from_iter(entries.iter().map(|&s| take_cstr(s)))
    } else {
        Vec::new()
    };
    SWSSStringArray_free(arr);
    out
}

pub(crate) fn make_field_value_array<I, F, V>(fvs: I) -> (SWSSFieldValueArray, KeepAlive)
where
    I: IntoIterator<Item = (F, V)>,
    F: AsRef<[u8]>,
    V: Into<CxxString>,
{
    let mut k = KeepAlive::default();
    let mut data = Vec::new();

    for (field, value) in fvs {
        let field = cstr(field);
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
        len: data.len().try_into().unwrap(),
    };
    k.keep(data);

    (arr, k)
}

pub(crate) fn make_key_op_field_values_array<I>(kfvs: I) -> (SWSSKeyOpFieldValuesArray, KeepAlive)
where
    I: IntoIterator<Item = KeyOpFieldValues>,
{
    let mut k = KeepAlive::default();
    let mut data = Vec::new();

    for kfv in kfvs {
        let key = cstr(kfv.key);
        let operation = kfv.operation.as_raw();
        let (field_values, arr_k) = make_field_value_array(kfv.field_values);
        data.push(SWSSKeyOpFieldValues {
            key: key.as_ptr(),
            operation,
            fieldValues: field_values,
        });
        k.keep(Box::new((key, arr_k)))
    }

    let arr = SWSSKeyOpFieldValuesArray {
        data: data.as_mut_ptr(),
        len: data.len().try_into().unwrap(),
    };
    k.keep(Box::new(data));

    (arr, k)
}

/// Helper struct to keep rust-owned data alive while it is in use by C++
#[derive(Default)]
pub(crate) struct KeepAlive(Vec<Box<dyn Any>>);

impl KeepAlive {
    fn keep<T: Any>(&mut self, t: T) {
        self.0.push(Box::new(t))
    }
}
