use super::*;
use crate::bindings::*;

/// Rust wrapper around `swss::ProducerStateTable`.
#[derive(Debug)]
pub struct ProducerStateTable {
    ptr: SWSSProducerStateTable,
    _db: DbConnector,
}

impl ProducerStateTable {
    pub fn new(db: DbConnector, table_name: &str) -> Result<Self> {
        let table_name = cstr(table_name);
        let ptr = unsafe { swss_try!(p_pst => SWSSProducerStateTable_new(db.ptr, table_name.as_ptr(), p_pst))? };
        Ok(Self { ptr, _db: db })
    }

    pub fn set_buffered(&self, buffered: bool) -> Result<()> {
        unsafe { swss_try!(SWSSProducerStateTable_setBuffered(self.ptr, buffered as u8)) }
    }

    pub fn set<I, F, V>(&self, key: &str, fvs: I) -> Result<()>
    where
        I: IntoIterator<Item = (F, V)>,
        F: AsRef<[u8]>,
        V: Into<CxxString>,
    {
        let key = cstr(key);
        let (arr, _k) = make_field_value_array(fvs);
        unsafe { swss_try!(SWSSProducerStateTable_set(self.ptr, key.as_ptr(), arr)) }
    }

    pub fn del(&self, key: &str) -> Result<()> {
        let key = cstr(key);
        unsafe { swss_try!(SWSSProducerStateTable_del(self.ptr, key.as_ptr())) }
    }

    pub fn flush(&self) -> Result<()> {
        unsafe { swss_try!(SWSSProducerStateTable_flush(self.ptr)) }
    }

    pub fn count(&self) -> Result<i64> {
        unsafe { swss_try!(p_count => SWSSProducerStateTable_count(self.ptr, p_count)) }
    }

    pub fn clear(&self) -> Result<()> {
        unsafe { swss_try!(SWSSProducerStateTable_clear(self.ptr)) }
    }

    pub fn create_temp_view(&self) -> Result<()> {
        unsafe { swss_try!(SWSSProducerStateTable_create_temp_view(self.ptr)) }
    }

    pub fn apply_temp_view(&self) -> Result<()> {
        unsafe { swss_try!(SWSSProducerStateTable_apply_temp_view(self.ptr)) }
    }
}

impl Drop for ProducerStateTable {
    fn drop(&mut self) {
        unsafe { swss_try!(SWSSProducerStateTable_free(self.ptr)).expect("Dropping ProducerStateTable") };
    }
}

unsafe impl Send for ProducerStateTable {}

#[cfg(feature = "async")]
impl ProducerStateTable {
    async_util::impl_basic_async_method!(
        set_async <= set<I, F, V>(&self, key: &str, fvs: I) -> Result<()>
                     where
                         I: IntoIterator<Item = (F, V)> + Send,
                         F: AsRef<[u8]>,
                         V: Into<CxxString>,
    );
    async_util::impl_basic_async_method!(del_async <= del(&self, key: &str) -> Result<()>);
    async_util::impl_basic_async_method!(flush_async <= flush(&self) -> Result<()>);
}
