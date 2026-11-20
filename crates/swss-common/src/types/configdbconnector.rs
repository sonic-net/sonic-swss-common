use super::*;
use crate::bindings::*;
use std::{
    collections::HashMap,
    ffi::{CStr, CString},
};

/// Raw CONFIG_DB update map accepted by the native bulk writer.
pub type ConfigDbRawMap = HashMap<String, HashMap<String, HashMap<String, CxxString>>>;

/// Rust wrapper around the existing `swss::ConfigDBPipeConnector_Native`.
#[derive(Debug)]
pub struct ConfigDBPipeConnector {
    ptr: SWSSConfigDBPipeConnector,
    use_unix_socket_path: bool,
    netns: String,
}

impl ConfigDBPipeConnector {
    pub fn new(use_unix_socket_path: bool, netns: Option<String>) -> Result<Self> {
        let netns = netns.unwrap_or_default();
        let netns_cstr = cstr(&netns)?;
        let ptr = unsafe {
            swss_try!(ptr => SWSSConfigDBPipeConnector_new(
                use_unix_socket_path as u8,
                netns_cstr.as_ptr(),
                ptr
            ))?
        };
        Ok(Self {
            ptr,
            use_unix_socket_path,
            netns,
        })
    }

    pub fn connect(&self, wait_for_init: bool, retry_on: bool) -> Result<()> {
        unsafe {
            swss_try!(SWSSConfigDBPipeConnector_connect(
                self.ptr,
                wait_for_init as u8,
                retry_on as u8
            ))
        }
    }

    /// Apply a complete update through the existing native `mod_config` path.
    pub fn mod_config_raw(&self, config: &ConfigDbRawMap) -> Result<()> {
        let borrowed = BorrowedConfigMap::new(config)?;
        unsafe {
            swss_try!(SWSSConfigDBPipeConnector_mod_config(
                self.ptr,
                &borrowed.raw
            ))
        }
    }

    /// Read the complete configuration through the native SCAN/pipeline path.
    pub fn get_config(
        &self,
    ) -> Result<HashMap<String, HashMap<String, HashMap<String, CxxString>>>> {
        unsafe {
            let config = swss_try!(config => SWSSConfigDBPipeConnector_get_config(
                self.ptr,
                config
            ))?;
            Ok(take_config_map(config))
        }
    }

    pub fn use_unix_socket_path(&self) -> bool {
        self.use_unix_socket_path
    }

    pub fn netns(&self) -> &str {
        &self.netns
    }
}

impl Drop for ConfigDBPipeConnector {
    fn drop(&mut self) {
        let _ = unsafe { swss_try!(SWSSConfigDBPipeConnector_free(self.ptr)) };
    }
}

unsafe impl Send for ConfigDBPipeConnector {}

struct BorrowedConfigMap {
    raw: SWSSConfigMap,
    _table_names: Vec<CString>,
    _row_names: Vec<CString>,
    _field_names: Vec<CString>,
    _field_arrays: Vec<Box<[SWSSFieldValueTuple]>>,
    _row_arrays: Vec<Box<[SWSSKeyOpFieldValues]>>,
    _tables: Box<[SWSSConfigTable]>,
}

impl BorrowedConfigMap {
    fn new(config: &ConfigDbRawMap) -> Result<Self> {
        let row_count = config.values().map(HashMap::len).sum();
        let field_count = config
            .values()
            .flat_map(HashMap::values)
            .map(HashMap::len)
            .sum();

        let mut table_names = Vec::with_capacity(config.len());
        let mut row_names = Vec::with_capacity(row_count);
        let mut field_names = Vec::with_capacity(field_count);
        let mut field_arrays = Vec::with_capacity(row_count);
        let mut row_arrays = Vec::with_capacity(config.len());
        let mut tables = Vec::with_capacity(config.len());

        for (table_name, rows) in config {
            table_names.push(cstr(table_name)?);
            let table_name_ptr = table_names.last().expect("just pushed").as_ptr();
            let mut raw_rows = Vec::with_capacity(rows.len());

            for (row_name, fields) in rows {
                row_names.push(cstr(row_name)?);
                let row_name_ptr = row_names.last().expect("just pushed").as_ptr();
                let mut raw_fields = Vec::with_capacity(fields.len());

                for (field_name, value) in fields {
                    field_names.push(cstr(field_name)?);
                    raw_fields.push(SWSSFieldValueTuple {
                        field: field_names.last().expect("just pushed").as_ptr(),
                        value: value.as_cxx_str().as_raw().cast(),
                    });
                }

                let mut raw_fields = raw_fields.into_boxed_slice();
                let field_values = SWSSFieldValueArray {
                    len: raw_fields.len().try_into().map_err(|_| {
                        Exception::new("CONFIG_DB field count exceeds uint64".to_string())
                    })?,
                    data: raw_fields.as_mut_ptr(),
                };
                field_arrays.push(raw_fields);
                raw_rows.push(SWSSKeyOpFieldValues {
                    key: row_name_ptr,
                    operation: SWSSKeyOperation_SWSSKeyOperation_SET,
                    fieldValues: field_values,
                });
            }

            let mut raw_rows = raw_rows.into_boxed_slice();
            let entries = SWSSKeyOpFieldValuesArray {
                len: raw_rows.len().try_into().map_err(|_| {
                    Exception::new("CONFIG_DB row count exceeds uint64".to_string())
                })?,
                data: raw_rows.as_mut_ptr(),
            };
            row_arrays.push(raw_rows);
            tables.push(SWSSConfigTable {
                table_name: table_name_ptr,
                entries,
            });
        }

        let mut tables = tables.into_boxed_slice();
        let raw = SWSSConfigMap {
            len: tables
                .len()
                .try_into()
                .map_err(|_| Exception::new("CONFIG_DB table count exceeds uint64".to_string()))?,
            data: tables.as_mut_ptr(),
        };
        Ok(Self {
            raw,
            _table_names: table_names,
            _row_names: row_names,
            _field_names: field_names,
            _field_arrays: field_arrays,
            _row_arrays: row_arrays,
            _tables: tables,
        })
    }
}

unsafe fn take_config_map(config: SWSSConfigMap) -> ConfigDbRawMap {
    let mut result = HashMap::new();

    for table_index in 0..config.len as usize {
        let table = &*config.data.add(table_index);
        let table_name = CStr::from_ptr(table.table_name)
            .to_string_lossy()
            .into_owned();
        let mut rows = HashMap::new();

        for row_index in 0..table.entries.len as usize {
            let row = &*table.entries.data.add(row_index);
            let row_name = CStr::from_ptr(row.key).to_string_lossy().into_owned();
            let mut fields = HashMap::new();

            for field_index in 0..row.fieldValues.len as usize {
                let field = &*row.fieldValues.data.add(field_index);
                let field_name = CStr::from_ptr(field.field).to_string_lossy().into_owned();
                let value = CStr::from_ptr(SWSSStrRef_c_str(field.value as SWSSStrRef))
                    .to_string_lossy()
                    .into_owned();
                fields.insert(field_name, CxxString::new(value));
            }

            rows.insert(row_name, fields);
        }

        result.insert(table_name, rows);
    }

    SWSSConfigMap_free(config);
    result
}

/// Rust wrapper around `swss::ConfigDBConnector_Native`.
#[derive(Debug)]
pub struct ConfigDBConnector {
    pub(crate) ptr: SWSSConfigDBConnector,
    use_unix_socket_path: bool,
    netns: String,
}

impl ConfigDBConnector {
    /// Create a new ConfigDBConnector.
    pub fn new(use_unix_socket_path: bool, netns: Option<String>) -> Result<ConfigDBConnector> {
        Self::new_with_auth(use_unix_socket_path, netns, false)
    }

    /// Explicit native authentication, with transport selected by the caller.
    pub fn new_with_auth(
        use_unix_socket_path: bool,
        netns: Option<String>,
        authenticate: bool
    ) -> Result<ConfigDBConnector> {
        let netns_str = netns.unwrap_or_default();
        let netns_cstr = cstr(&netns_str)?;

        let ptr = unsafe {
            swss_try!(p_config_db => SWSSConfigDBConnector_new_with_auth(
                use_unix_socket_path as u8,
                netns_cstr.as_ptr(),
                authenticate as u8,
                p_config_db
            ))?
        };

        Ok(Self {
            ptr,
            use_unix_socket_path,
            netns: netns_str,
        })
    }

    /// Connect to ConfigDB.
    ///
    /// # Arguments
    /// * `wait_for_init` - Wait for CONFIG_DB_INITIALIZED flag if true
    /// * `retry_on` - Retry connection on failure if true
    pub fn connect(&self, wait_for_init: bool, retry_on: bool) -> Result<()> {
        unsafe {
            swss_try!(SWSSConfigDBConnector_connect(
                self.ptr,
                wait_for_init as u8,
                retry_on as u8
            ))
        }
    }

    /// Get a single entry from a table.
    pub fn get_entry(&self, table: &str, key: &str) -> Result<HashMap<String, CxxString>> {
        let table_cstr = cstr(table)?;
        let key_cstr = cstr(key)?;

        unsafe {
            let arr = swss_try!(p_arr => SWSSConfigDBConnector_get_entry(
                self.ptr,
                table_cstr.as_ptr(),
                key_cstr.as_ptr(),
                p_arr
            ))?;
            take_field_value_array(arr)
        }
    }

    /// Get all keys from a table.
    pub fn get_keys(&self, table: &str, split: bool) -> Result<Vec<String>> {
        let table_cstr = cstr(table)?;

        unsafe {
            let arr = swss_try!(p_arr => SWSSConfigDBConnector_get_keys(
                self.ptr,
                table_cstr.as_ptr(),
                split as u8,
                p_arr
            ))?;
            take_string_array(arr)
        }
    }

    /// Get entire table as key-value pairs.
    /// Returns a HashMap where keys are table keys and values are field-value maps.
    pub fn get_table(&self, table: &str) -> Result<HashMap<String, HashMap<String, CxxString>>> {
        let table_cstr = cstr(table)?;

        unsafe {
            let arr = swss_try!(p_arr => SWSSConfigDBConnector_get_table(
                self.ptr,
                table_cstr.as_ptr(),
                p_arr
            ))?;

            let kfvs = take_key_op_field_values_array(arr)?;
            let mut table_map = HashMap::new();

            for kfv in kfvs {
                table_map.insert(kfv.key, kfv.field_values);
            }

            Ok(table_map)
        }
    }

    /// Set an entry in a table.
    pub fn set_entry<I, F, V>(&self, table: &str, key: &str, data: I) -> Result<()>
    where
        I: IntoIterator<Item = (F, V)>,
        F: AsRef<[u8]>,
        V: Into<CxxString>,
    {
        let table_cstr = cstr(table)?;
        let key_cstr = cstr(key)?;
        let (fv_array, _keep_alive) = make_field_value_array(data)?;

        unsafe {
            swss_try!(SWSSConfigDBConnector_set_entry(
                self.ptr,
                table_cstr.as_ptr(),
                key_cstr.as_ptr(),
                &fv_array
            ))
        }
    }

    /// Modify an entry in a table (update existing fields, add new ones).
    pub fn mod_entry<I, F, V>(&self, table: &str, key: &str, data: I) -> Result<()>
    where
        I: IntoIterator<Item = (F, V)>,
        F: AsRef<[u8]>,
        V: Into<CxxString>,
    {
        let table_cstr = cstr(table)?;
        let key_cstr = cstr(key)?;
        let (fv_array, _keep_alive) = make_field_value_array(data)?;

        unsafe {
            swss_try!(SWSSConfigDBConnector_mod_entry(
                self.ptr,
                table_cstr.as_ptr(),
                key_cstr.as_ptr(),
                &fv_array
            ))
        }
    }

    /// Delete an entire table.
    pub fn delete_table(&self, table: &str) -> Result<()> {
        let table_cstr = cstr(table)?;

        unsafe {
            swss_try!(SWSSConfigDBConnector_delete_table(
                self.ptr,
                table_cstr.as_ptr()
            ))
        }
    }

    /// Get connection info.
    pub fn use_unix_socket_path(&self) -> bool {
        self.use_unix_socket_path
    }

    /// Get netns.
    pub fn netns(&self) -> &str {
        &self.netns
    }

    /// Get the Redis client for the specified database.
    /// This allows direct Redis operations on the underlying database.
    /// Returns a borrowed reference to the underlying Redis client.
    pub fn get_redis_client(&self, db_name: &str) -> Result<BorrowedDbConnector> {
        let db_name_cstr = cstr(db_name)?;

        unsafe {
            let db_connector_ptr = swss_try!(p_db => SWSSConfigDBConnector_get_redis_client(
                self.ptr,
                db_name_cstr.as_ptr(),
                p_db
            ))?;

            // Create a borrowed wrapper around the returned pointer
            // Note: This is a reference to an existing connector, so we don't own it
            Ok(BorrowedDbConnector {
                ptr: db_connector_ptr,
            })
        }
    }

    /// Get the entire configuration as a nested map structure.
    /// Returns a HashMap where keys are table names and values are table contents
    /// (key -> field -> value mappings).
    pub fn get_config(
        &self,
    ) -> Result<HashMap<String, HashMap<String, HashMap<String, CxxString>>>> {
        unsafe {
            let config = swss_try!(config => SWSSConfigDBConnector_get_config(
                self.ptr,
                config
            ))?;
            Ok(take_config_map(config))
        }
    }
}

impl Drop for ConfigDBConnector {
    fn drop(&mut self) {
        // Ignore errors in Drop to avoid panic-during-panic scenarios
        let _ = unsafe { swss_try!(SWSSConfigDBConnector_free(self.ptr)) };
    }
}

unsafe impl Send for ConfigDBConnector {}

#[cfg(feature = "async")]
impl ConfigDBConnector {
    async_util::impl_basic_async_method!(new_async <= new(use_unix_socket_path: bool, netns: Option<String>) -> Result<ConfigDBConnector>);
    async_util::impl_basic_async_method!(connect_async <= connect(&self, wait_for_init: bool, retry_on: bool) -> Result<()>);
    async_util::impl_basic_async_method!(get_entry_async <= get_entry(&self, table: &str, key: &str) -> Result<HashMap<String, CxxString>>);
    async_util::impl_basic_async_method!(get_keys_async <= get_keys(&self, table: &str, split: bool) -> Result<Vec<String>>);
    async_util::impl_basic_async_method!(get_table_async <= get_table(&self, table: &str) -> Result<HashMap<String, HashMap<String, CxxString>>>);
    async_util::impl_basic_async_method!(get_config_async <= get_config(&self) -> Result<HashMap<String, HashMap<String, HashMap<String, CxxString>>>>);
    async_util::impl_basic_async_method!(delete_table_async <= delete_table(&self, table: &str) -> Result<()>);
}

/// A borrowed reference to a DbConnector obtained from ConfigDBConnector.
/// This does not own the underlying connection and does not implement Drop.
#[derive(Debug)]
pub struct BorrowedDbConnector {
    ptr: SWSSDBConnector,
}

impl BorrowedDbConnector {
    /// Create a new BorrowedDbConnector from a raw pointer.
    /// This is intended for internal use by other modules.
    pub(crate) fn new(ptr: SWSSDBConnector) -> Self {
        Self { ptr }
    }

    /// Flush the current database, removing all keys.
    /// Equivalent to Redis FLUSHDB command.
    pub fn flush_db(&self) -> Result<bool> {
        let status = unsafe { swss_try!(p_status => SWSSDBConnector_flushdb(self.ptr, p_status))? };
        Ok(status == 1)
    }
}
