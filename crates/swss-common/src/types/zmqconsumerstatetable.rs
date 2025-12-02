use super::*;
use crate::bindings::*;
use std::{os::fd::BorrowedFd, ptr::null, sync::Arc, time::Duration};

/// Rust wrapper around `swss::ZmqConsumerStateTable`.
#[derive(Debug)]
pub struct ZmqConsumerStateTable {
    ptr: SWSSZmqConsumerStateTable,
    _db: DbConnector,

    /// See [`DropGuard`] and [`ZmqServer`].
    _drop_guard: Arc<DropGuard>,
}

impl ZmqConsumerStateTable {
    pub fn new(
        db: DbConnector,
        table_name: &str,
        zmqs: &mut ZmqServer,
        pop_batch_size: Option<i32>,
        pri: Option<i32>,
    ) -> Result<Self> {
        let table_name = cstr(table_name);
        let pop_batch_size = pop_batch_size.as_ref().map(|n| n as *const i32).unwrap_or(null());
        let pri = pri.as_ref().map(|n| n as *const i32).unwrap_or(null());
        let ptr = unsafe {
            swss_try!(p_zs => {
                SWSSZmqConsumerStateTable_new(db.ptr, table_name.as_ptr(), zmqs.ptr, pop_batch_size, pri, p_zs)
            })?
        };
        let drop_guard = Arc::new(DropGuard(ptr));
        zmqs.register_consumer_state_table(drop_guard.clone());
        Ok(Self {
            ptr,
            _db: db,
            _drop_guard: drop_guard,
        })
    }

    pub fn pops(&self) -> Result<Vec<KeyOpFieldValues>> {
        unsafe {
            let arr = swss_try!(p_arr => SWSSZmqConsumerStateTable_pops(self.ptr, p_arr))?;
            Ok(take_key_op_field_values_array(arr))
        }
    }

    pub fn get_fd(&self) -> Result<BorrowedFd> {
        // SAFETY: This fd represents the underlying zmq socket, which should remain alive as long as there
        // is a listener (i.e. a ZmqConsumerStateTable)
        unsafe {
            let fd = swss_try!(p_fd => SWSSZmqConsumerStateTable_getFd(self.ptr, p_fd))?;
            let fd = BorrowedFd::borrow_raw(fd.try_into().unwrap());
            Ok(fd)
        }
    }

    pub fn read_data(&self, timeout: Duration, interrupt_on_signal: bool) -> Result<SelectResult> {
        let timeout_ms = timeout.as_millis().try_into().unwrap();
        let res = unsafe {
            swss_try!(p_res =>
                SWSSZmqConsumerStateTable_readData(self.ptr, timeout_ms, interrupt_on_signal as u8, p_res)
            )?
        };
        Ok(SelectResult::from_raw(res))
    }
}

unsafe impl Send for ZmqConsumerStateTable {}

/// A type that will free the underlying `ZmqConsumerStateTable` when it is dropped.
/// This is shared with `ZmqServer`
#[derive(Debug)]
pub(crate) struct DropGuard(SWSSZmqConsumerStateTable);

impl Drop for DropGuard {
    fn drop(&mut self) {
        unsafe { swss_try!(SWSSZmqConsumerStateTable_free(self.0)).expect("Dropping ZmqConsumerStateTable") };
    }
}

// SAFETY: This is safe as long as ZmqConsumerStateTable is Send
unsafe impl Send for DropGuard {}

// SAFETY: There is no way to use &DropGuard so it is safe
unsafe impl Sync for DropGuard {}

/// Async versions of methods
#[cfg(feature = "async")]
impl ZmqConsumerStateTable {
    async_util::impl_read_data_async!();
    async_util::impl_basic_async_method!(pops_async <= pops(&self) -> Result<Vec<KeyOpFieldValues>>);
}
