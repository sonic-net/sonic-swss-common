use super::*;
use crate::bindings::*;

/// Rust wrapper around `swss::ZmqProducerStateTable`.
#[derive(Debug)]
pub struct ZmqProducerStateTable {
    ptr: SWSSZmqProducerStateTable,
    _db: DbConnector,
    _zmqc: ZmqClient,
}

impl ZmqProducerStateTable {
    pub fn new(db: DbConnector, table_name: &str, zmqc: ZmqClient, db_persistence: bool) -> Result<Self> {
        let table_name = cstr(table_name);
        let db_persistence = db_persistence as u8;
        let ptr = unsafe {
            swss_try!(p_zpst =>
                SWSSZmqProducerStateTable_new(db.ptr, table_name.as_ptr(), zmqc.ptr, db_persistence, p_zpst)
            )?
        };
        Ok(Self {
            ptr,
            _db: db,
            _zmqc: zmqc,
        })
    }

    pub fn set<I, F, V>(&self, key: &str, fvs: I) -> Result<()>
    where
        I: IntoIterator<Item = (F, V)>,
        F: AsRef<[u8]>,
        V: Into<CxxString>,
    {
        let key = cstr(key);
        let (arr, _k) = make_field_value_array(fvs);
        unsafe { swss_try!(SWSSZmqProducerStateTable_set(self.ptr, key.as_ptr(), arr)) }
    }

    pub fn del(&self, key: &str) -> Result<()> {
        let key = cstr(key);
        unsafe { swss_try!(SWSSZmqProducerStateTable_del(self.ptr, key.as_ptr())) }
    }

    pub fn db_updater_queue_size(&self) -> Result<u64> {
        unsafe { swss_try!(p_size => SWSSZmqProducerStateTable_dbUpdaterQueueSize(self.ptr, p_size)) }
    }
}

impl Drop for ZmqProducerStateTable {
    fn drop(&mut self) {
        unsafe { SWSSZmqProducerStateTable_free(self.ptr) };
    }
}

unsafe impl Send for ZmqProducerStateTable {}

#[cfg(feature = "async")]
impl ZmqProducerStateTable {
    async_util::impl_basic_async_method!(
        set_async <= set<I, F, V>(&self, key: &str, fvs: I) -> Result<()>
                     where
                         I: IntoIterator<Item = (F, V)> + Send,
                         F: AsRef<[u8]>,
                         V: Into<CxxString>,
    );
    async_util::impl_basic_async_method!(del_async <= del(&self, key: &str) -> Result<()>);
}
