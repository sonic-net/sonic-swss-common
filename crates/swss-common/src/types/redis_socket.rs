use super::*;
use std::os::fd::{FromRawFd, OwnedFd};

/// Open a Unix Redis connection with native policy matching and writer login.
/// No logical database is selected. Zero timeouts mean no timeout.
/// The returned descriptor is close-on-exec and closes when dropped.
pub fn open_redis_unix_socket(path: &str, connect_timeout_ms: u32, auth_timeout_ms: u32) -> Result<OwnedFd> {
    let path = cstr(path)?;
    let fd = unsafe {
        swss_try!(fd => SWSSRedisSocket_open_unix(path.as_ptr(), connect_timeout_ms, auth_timeout_ms, fd))?
    };
    Ok(unsafe { OwnedFd::from_raw_fd(fd) })
}
