use super::*;
use crate::bindings::*;
use std::{os::fd::BorrowedFd, ptr::null, time::Duration};

/// Rust wrapper around `swss::SubscriberStateTable`.
#[derive(Debug)]
pub struct SubscriberStateTable {
    ptr: SWSSSubscriberStateTable,
    db: DbConnector,
    table_name: String,
}

impl SubscriberStateTable {
    pub fn new(db: DbConnector, table_name: &str, pop_batch_size: Option<i32>, pri: Option<i32>) -> Result<Self> {
        let table_name_string = String::from(table_name);
        let table_name = cstr(table_name);
        let pop_batch_size = pop_batch_size.as_ref().map(|n| n as *const i32).unwrap_or(null());
        let pri = pri.as_ref().map(|n| n as *const i32).unwrap_or(null());
        let ptr = unsafe {
            swss_try!(p_sst => {
                SWSSSubscriberStateTable_new(db.ptr, table_name.as_ptr(), pop_batch_size, pri, p_sst)
            })?
        };
        Ok(Self {
            ptr,
            db,
            table_name: table_name_string,
        })
    }

    pub fn pops(&self) -> Result<Vec<KeyOpFieldValues>> {
        unsafe {
            let arr = swss_try!(p_arr => SWSSSubscriberStateTable_pops(self.ptr, p_arr))?;
            Ok(take_key_op_field_values_array(arr))
        }
    }

    pub fn read_data(&self, timeout: Duration, interrupt_on_signal: bool) -> Result<SelectResult> {
        let timeout_ms = timeout.as_millis().try_into().unwrap();
        let res = unsafe {
            swss_try!(p_res => {
                SWSSSubscriberStateTable_readData(self.ptr, timeout_ms, interrupt_on_signal as u8, p_res)
            })?
        };
        Ok(SelectResult::from_raw(res))
    }

    pub fn get_fd(&self) -> Result<BorrowedFd> {
        // SAFETY: This fd represents the underlying redis connection, which should stay alive
        // as long as the DbConnector does.
        unsafe {
            let fd = swss_try!(p_fd => SWSSSubscriberStateTable_getFd(self.ptr, p_fd))?;
            let fd = BorrowedFd::borrow_raw(fd.try_into().unwrap());
            Ok(fd)
        }
    }

    pub fn db_connector(&self) -> &DbConnector {
        &self.db
    }

    pub fn db_connector_mut(&mut self) -> &mut DbConnector {
        &mut self.db
    }

    pub fn table_name(&self) -> &str {
        &self.table_name
    }
}

impl Drop for SubscriberStateTable {
    fn drop(&mut self) {
        unsafe { swss_try!(SWSSSubscriberStateTable_free(self.ptr)).expect("Dropping SubscriberStateTable") };
    }
}

unsafe impl Send for SubscriberStateTable {}

/// Async versions of methods
#[cfg(feature = "async")]
impl SubscriberStateTable {
    async_util::impl_read_data_async!();
    async_util::impl_basic_async_method!(new_async <= new(db: DbConnector, table_name: &str, pop_batch_size: Option<i32>, pri: Option<i32>) -> Result<Self>);
    async_util::impl_basic_async_method!(pops_async <= pops(&self) -> Result<Vec<KeyOpFieldValues>>);
}
