use super::*;
use crate::bindings::*;

/// Rust wrapper around `swss::ZmqClient`.
#[derive(Debug)]
pub struct ZmqClient {
    pub(crate) ptr: SWSSZmqClient,
}

impl ZmqClient {
    pub fn new(endpoint: &str) -> Result<Self> {
        let endpoint = cstr(endpoint);
        let ptr = unsafe { swss_try!(p_zc => SWSSZmqClient_new(endpoint.as_ptr(), p_zc))? };
        Ok(Self { ptr })
    }

    pub fn is_connected(&self) -> Result<bool> {
        let status = unsafe { swss_try!(p_status => SWSSZmqClient_isConnected(self.ptr, p_status))? };
        Ok(status == 1)
    }

    pub fn connect(&self) -> Result<()> {
        unsafe { swss_try!(SWSSZmqClient_connect(self.ptr)) }
    }

    pub fn send_msg<I>(&self, db_name: &str, table_name: &str, kfvs: I) -> Result<()>
    where
        I: IntoIterator<Item = KeyOpFieldValues>,
    {
        let db_name = cstr(db_name);
        let table_name = cstr(table_name);
        let (kfvs, _k) = make_key_op_field_values_array(kfvs);
        unsafe {
            swss_try!(SWSSZmqClient_sendMsg(
                self.ptr,
                db_name.as_ptr(),
                table_name.as_ptr(),
                kfvs,
            ))
        }
    }
}

impl Drop for ZmqClient {
    fn drop(&mut self) {
        unsafe { swss_try!(SWSSZmqClient_free(self.ptr)).expect("Dropping ZmqClient") };
    }
}

unsafe impl Send for ZmqClient {}

#[cfg(feature = "async")]
impl ZmqClient {
    async_util::impl_basic_async_method!(new_async <= new(endpoint: &str) -> Result<Self>);
    async_util::impl_basic_async_method!(connect_async <= connect(&self) -> Result<()>);
    async_util::impl_basic_async_method!(
        send_msg_async <= send_msg<I>(&self, db_name: &str, table_name: &str, kfvs: I) -> Result<()>
                          where
                              I: IntoIterator<Item = KeyOpFieldValues> + Send,
    );
}
