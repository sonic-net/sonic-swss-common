use super::*;
use crate::bindings::*;
use std::{os::fd::BorrowedFd, ptr::null};

/// Rust wrapper around `swss::ConsumerStateTable`.
#[derive(Debug)]
pub struct ConsumerStateTable {
    ptr: SWSSConsumerStateTable,
    db: DbConnector,
    table_name: String,
}

impl ConsumerStateTable {
    pub fn new(db: DbConnector, table_name: &str, pop_batch_size: Option<i32>, pri: Option<i32>) -> Result<Self> {
        let table_name_string = String::from(table_name);
        let table_name = cstr(table_name);
        let pop_batch_size = pop_batch_size.as_ref().map(|n| n as *const i32).unwrap_or(null());
        let pri = pri.as_ref().map(|n| n as *const i32).unwrap_or(null());
        let ptr = unsafe {
            swss_try!(p_cst => SWSSConsumerStateTable_new(db.ptr, table_name.as_ptr(), pop_batch_size, pri, p_cst))?
        };
        Ok(Self {
            ptr,
            db,
            table_name: table_name_string,
        })
    }

    pub fn pops(&self) -> Result<Vec<KeyOpFieldValues>> {
        unsafe {
            let arr = swss_try!(p_arr => SWSSConsumerStateTable_pops(self.ptr, p_arr))?;
            Ok(take_key_op_field_values_array(arr))
        }
    }

    pub fn get_fd(&self) -> std::io::Result<BorrowedFd> {
        // SAFETY: This fd represents the underlying redis connection, which should stay alive
        // as long as the DbConnector does.
        unsafe {
            let fd = swss_try!(p_fd => SWSSConsumerStateTable_getFd(self.ptr, p_fd))
                .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e.to_string()))?;
            if fd == -1 {
                return Err(std::io::Error::new(std::io::ErrorKind::InvalidInput, "Invalid file descriptor"));
            }
            let fd = BorrowedFd::borrow_raw(fd);
            Ok(fd)
        }
    }

    pub fn read_data(&self, timeout_ms: u32, interrupt_on_signal: bool) -> Result<SelectResult> {
        let res = unsafe {
            swss_try!(p_res => {
                SWSSConsumerStateTable_readData(self.ptr, timeout_ms, interrupt_on_signal as u8, p_res)
            })?
        };
        Ok(SelectResult::from_raw(res))
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

impl Drop for ConsumerStateTable {
    fn drop(&mut self) {
        unsafe {
            if let Err(e) = swss_try!(SWSSConsumerStateTable_free(self.ptr)) {
                eprintln!("Error dropping ConsumerStateTable: {}", e);
            }
        }
    }
}

unsafe impl Send for ConsumerStateTable {}

/// Async versions of methods
#[cfg(feature = "async")]
impl ConsumerStateTable {
    async_util::impl_read_data_async!();
    async_util::impl_basic_async_method!(pops_async <= pops(&self) -> Result<Vec<KeyOpFieldValues>>);
}
