use super::*;
use crate::bindings::*;
use std::collections::HashMap;

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
        let netns_str = netns.unwrap_or_default();
        let netns_cstr = cstr(&netns_str);
        
        let ptr = unsafe {
            swss_try!(p_config_db => SWSSConfigDBConnector_new(
                use_unix_socket_path as u8, 
                netns_cstr.as_ptr(), 
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
        let table_cstr = cstr(table);
        let key_cstr = cstr(key);
        
        unsafe {
            let arr = swss_try!(p_arr => SWSSConfigDBConnector_get_entry(
                self.ptr, 
                table_cstr.as_ptr(), 
                key_cstr.as_ptr(), 
                p_arr
            ))?;
            Ok(take_field_value_array(arr))
        }
    }

    /// Get all keys from a table.
    pub fn get_keys(&self, table: &str, split: bool) -> Result<Vec<String>> {
        let table_cstr = cstr(table);
        
        unsafe {
            let arr = swss_try!(p_arr => SWSSConfigDBConnector_get_keys(
                self.ptr, 
                table_cstr.as_ptr(), 
                split as u8, 
                p_arr
            ))?;
            Ok(take_string_array(arr))
        }
    }

    /// Get entire table as key-value pairs.
    /// Returns a HashMap where keys are table keys and values are field-value maps.
    pub fn get_table(&self, table: &str) -> Result<HashMap<String, HashMap<String, CxxString>>> {
        let table_cstr = cstr(table);
        
        unsafe {
            let arr = swss_try!(p_arr => SWSSConfigDBConnector_get_table(
                self.ptr, 
                table_cstr.as_ptr(), 
                p_arr
            ))?;
            
            let kfvs = take_key_op_field_values_array(arr);
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
        let table_cstr = cstr(table);
        let key_cstr = cstr(key);
        let (fv_array, _keep_alive) = make_field_value_array(data);
        
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
        let table_cstr = cstr(table);
        let key_cstr = cstr(key);
        let (fv_array, _keep_alive) = make_field_value_array(data);
        
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
        let table_cstr = cstr(table);
        
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
}

impl Drop for ConfigDBConnector {
    fn drop(&mut self) {
        unsafe { swss_try!(SWSSConfigDBConnector_free(self.ptr)).expect("Dropping ConfigDBConnector") };
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
    async_util::impl_basic_async_method!(delete_table_async <= delete_table(&self, table: &str) -> Result<()>);
}