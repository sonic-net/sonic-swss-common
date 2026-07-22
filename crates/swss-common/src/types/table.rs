use super::*;
use crate::bindings::*;
use std::ptr;

/// Rust wrapper around `swss::Table`.
#[derive(Debug)]
pub struct Table {
    name: String,
    ptr: SWSSTable,
    _db: DbConnector,
}

impl Table {
    pub fn new(db: DbConnector, table_name: &str) -> Result<Self> {
        let table_name_copy = table_name.to_string();
        let table_name = cstr(table_name);
        let ptr = unsafe { swss_try!(p_tbl => SWSSTable_new(db.ptr, table_name.as_ptr(), p_tbl))? };
        Ok(Self {
            name: table_name_copy,
            ptr,
            _db: db,
        })
    }

    pub fn get(&self, key: &str) -> Result<Option<FieldValues>> {
        let key = cstr(key);
        let mut arr = SWSSFieldValueArray {
            len: 0,
            data: ptr::null_mut(),
        };
        let exists = unsafe { swss_try!(p_exists => SWSSTable_get(self.ptr, key.as_ptr(), &mut arr, p_exists))? };
        let maybe_fvs = if exists == 1 {
            Some(unsafe { take_field_value_array(arr) })
        } else {
            None
        };
        Ok(maybe_fvs)
    }

    pub fn hget(&self, key: &str, field: &str) -> Result<Option<CxxString>> {
        let key = cstr(key);
        let field = cstr(field);
        let mut val: SWSSString = ptr::null_mut();
        let exists = unsafe {
            swss_try!(p_exists => SWSSTable_hget(self.ptr, key.as_ptr(), field.as_ptr(), &mut val, p_exists))?
        };
        let maybe_fvs = if exists == 1 {
            Some(unsafe { CxxString::take(val).unwrap() })
        } else {
            None
        };
        Ok(maybe_fvs)
    }

    pub fn set<I, F, V>(&self, key: &str, fvs: I) -> Result<()>
    where
        I: IntoIterator<Item = (F, V)>,
        F: AsRef<[u8]>,
        V: Into<CxxString>,
    {
        let key = cstr(key);
        let (arr, _k) = make_field_value_array(fvs);
        unsafe { swss_try!(SWSSTable_set(self.ptr, key.as_ptr(), arr)) }
    }

    pub fn hset(&self, key: &str, field: &str, val: &CxxStr) -> Result<()> {
        let key = cstr(key);
        let field = cstr(field);
        unsafe { swss_try!(SWSSTable_hset(self.ptr, key.as_ptr(), field.as_ptr(), val.as_raw())) }
    }

    pub fn del(&self, key: &str) -> Result<()> {
        let key = cstr(key);
        unsafe { swss_try!(SWSSTable_del(self.ptr, key.as_ptr())) }
    }

    pub fn hdel(&self, key: &str, field: &str) -> Result<()> {
        let key = cstr(key);
        let field = cstr(field);
        unsafe { swss_try!(SWSSTable_hdel(self.ptr, key.as_ptr(), field.as_ptr())) }
    }

    pub fn get_keys(&self) -> Result<Vec<String>> {
        unsafe {
            let arr = swss_try!(p_arr => SWSSTable_getKeys(self.ptr, p_arr))?;
            Ok(take_string_array(arr))
        }
    }

    pub fn get_name(&self) -> &str {
        &self.name
    }
}

impl Drop for Table {
    fn drop(&mut self) {
        unsafe { swss_try!(SWSSTable_free(self.ptr)).expect("Dropping Table") };
    }
}

unsafe impl Send for Table {}

#[cfg(feature = "async")]
impl Table {
    async_util::impl_basic_async_method!(new_async <= new(db: DbConnector, table_name: &str) -> Result<Self>);
    async_util::impl_basic_async_method!(get_async <= get(&self, key: &str) -> Result<Option<FieldValues>>);
    async_util::impl_basic_async_method!(hget_async <= hget(&self, key: &str, field: &str) -> Result<Option<CxxString>>);
    async_util::impl_basic_async_method!(
        set_async <= set<I, F, V>(&self, key: &str, fvs: I) -> Result<()>
                     where
                         I: IntoIterator<Item = (F, V)> + Send,
                         F: AsRef<[u8]>,
                         V: Into<CxxString>,
    );
    async_util::impl_basic_async_method!(hset_async <= hset(&self, key: &str, field: &str, value: &CxxStr) -> Result<()>);
    async_util::impl_basic_async_method!(del_async <= del(&self, key: &str) -> Result<()>);
    async_util::impl_basic_async_method!(hdel_async <= hdel(&self, key: &str, field: &str) -> Result<()>);
    async_util::impl_basic_async_method!(get_keys_async <= get_keys(&self) -> Result<Vec<String>>);
}
