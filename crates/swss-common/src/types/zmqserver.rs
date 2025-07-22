use super::*;
use crate::bindings::*;
use std::sync::Arc;

/// Rust wrapper around `swss::ZmqServer`.
#[derive(Debug)]
pub struct ZmqServer {
    pub(crate) ptr: SWSSZmqServer,

    /// The types that register message handlers with a ZmqServer must be kept alive until
    /// the server thread dies, otherwise we risk the server thread calling methods on deleted objects.
    ///
    /// Currently this is just ZmqConsumerStateTable, but in the future there may be other types added
    /// and this vec will need to hold an enum of the possible message handlers.
    message_handler_guards: Vec<Arc<zmqconsumerstatetable::DropGuard>>,
}

impl ZmqServer {
    pub fn new(endpoint: &str) -> Result<Self> {
        let endpoint = cstr(endpoint);
        let ptr = unsafe { swss_try!(p_zs => SWSSZmqServer_new(endpoint.as_ptr(), p_zs))? };
        Ok(Self {
            ptr,
            message_handler_guards: Vec::new(),
        })
    }

    pub(crate) fn register_consumer_state_table(&mut self, tbl_dg: Arc<zmqconsumerstatetable::DropGuard>) {
        self.message_handler_guards.push(tbl_dg);
    }
}

impl Drop for ZmqServer {
    fn drop(&mut self) {
        unsafe { SWSSZmqServer_free(self.ptr) };
    }
}

unsafe impl Send for ZmqServer {}
