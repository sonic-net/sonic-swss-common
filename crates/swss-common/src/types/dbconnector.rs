use super::*;
use crate::bindings::*;
use std::collections::HashMap;
use std::ffi::CString;

/// A non-secret reference to a named Redis authentication profile.
///
/// The profile resolves to protected credential metadata in C++. The credential
/// itself is never passed through the Rust or C APIs.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RedisAuthProfile {
    name: String,
    profiles_file: Option<String>,
}

impl RedisAuthProfile {
    /// Select a profile by name. Pass `None` to use the default profile file.
    pub fn new(name: impl Into<String>, profiles_file: Option<String>) -> Self {
        Self {
            name: name.into(),
            profiles_file,
        }
    }

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn profiles_file(&self) -> Option<&str> {
        self.profiles_file.as_deref()
    }

    pub(crate) fn as_c_strings(&self) -> Result<(CString, Option<CString>)> {
        Ok((
            cstr(&self.name)?,
            self.profiles_file.as_ref().map(cstr).transpose()?,
        ))
    }
}

/// Rust wrapper around `swss::DBConnector`.
#[derive(Debug)]
pub struct DbConnector {
    pub(crate) ptr: SWSSDBConnector,
    connection: DbConnectionInfo,
    auth_profile: Option<RedisAuthProfile>,
}

/// Details about how a DbConnector is connected to Redis
#[derive(Debug, Clone)]
pub enum DbConnectionInfo {
    Tcp {
        hostname: String,
        port: u16,
        db_id: i32,
    },
    Unix {
        sock_path: String,
        db_id: i32,
    },
    Named {
        db_name: String,
        is_tcp_conn: bool,
    },
    Keyed {
        db_name: String,
        is_tcp_conn: bool,
        container_name: String,
        netns: String,
    },
}

impl DbConnector {
    /// Create a new DbConnector from [`DbConnectionInfo`].
    ///
    /// Timeout of 0 means block indefinitely.
    pub fn new(connection: DbConnectionInfo, timeout_ms: u32) -> Result<DbConnector> {
        Self::new_internal(connection, timeout_ms, None)
    }

    /// Create a new DbConnector using a named Redis authentication profile.
    ///
    /// Timeout of 0 means block indefinitely.
    pub fn new_with_profile(
        connection: DbConnectionInfo,
        timeout_ms: u32,
        auth_profile: RedisAuthProfile,
    ) -> Result<DbConnector> {
        Self::new_internal(connection, timeout_ms, Some(auth_profile))
    }

    fn new_internal(
        connection: DbConnectionInfo,
        timeout_ms: u32,
        auth_profile: Option<RedisAuthProfile>,
    ) -> Result<DbConnector> {
        let profile_strings = auth_profile
            .as_ref()
            .map(RedisAuthProfile::as_c_strings)
            .transpose()?;

        let ptr = match &connection {
            DbConnectionInfo::Tcp {
                hostname,
                port,
                db_id,
            } => {
                let hostname = cstr(hostname)?;
                unsafe {
                    if let Some((profile, profiles_file)) = &profile_strings {
                        swss_try!(p_db => SWSSDBConnector_new_tcp_with_profile(
                            *db_id,
                            hostname.as_ptr(),
                            *port,
                            timeout_ms,
                            profile.as_ptr(),
                            profiles_file.as_ref().map_or(std::ptr::null(), |path| path.as_ptr()),
                            p_db
                        ))?
                    } else {
                        swss_try!(p_db => SWSSDBConnector_new_tcp(
                            *db_id,
                            hostname.as_ptr(),
                            *port,
                            timeout_ms,
                            p_db
                        ))?
                    }
                }
            }
            DbConnectionInfo::Unix { sock_path, db_id } => {
                let sock_path = cstr(sock_path)?;
                unsafe {
                    if let Some((profile, profiles_file)) = &profile_strings {
                        swss_try!(p_db => SWSSDBConnector_new_unix_with_profile(
                            *db_id,
                            sock_path.as_ptr(),
                            timeout_ms,
                            profile.as_ptr(),
                            profiles_file.as_ref().map_or(std::ptr::null(), |path| path.as_ptr()),
                            p_db
                        ))?
                    } else {
                        swss_try!(p_db => SWSSDBConnector_new_unix(
                            *db_id,
                            sock_path.as_ptr(),
                            timeout_ms,
                            p_db
                        ))?
                    }
                }
            }
            DbConnectionInfo::Named {
                db_name,
                is_tcp_conn,
            } => {
                let db_name = cstr(db_name)?;
                unsafe {
                    if let Some((profile, profiles_file)) = &profile_strings {
                        swss_try!(p_db => SWSSDBConnector_new_named_with_profile(
                            db_name.as_ptr(),
                            timeout_ms,
                            *is_tcp_conn as u8,
                            profile.as_ptr(),
                            profiles_file.as_ref().map_or(std::ptr::null(), |path| path.as_ptr()),
                            p_db
                        ))?
                    } else {
                        swss_try!(p_db => SWSSDBConnector_new_named(
                            db_name.as_ptr(),
                            timeout_ms,
                            *is_tcp_conn as u8,
                            p_db
                        ))?
                    }
                }
            }
            DbConnectionInfo::Keyed {
                db_name,
                is_tcp_conn,
                container_name,
                netns,
            } => {
                let db_name = cstr(db_name)?;
                let container_name = cstr(container_name)?;
                let netns = cstr(netns)?;
                unsafe {
                    if let Some((profile, profiles_file)) = &profile_strings {
                        swss_try!(p_db => SWSSDBConnector_new_keyed_with_profile(
                            db_name.as_ptr(),
                            timeout_ms,
                            *is_tcp_conn as u8,
                            container_name.as_ptr(),
                            netns.as_ptr(),
                            profile.as_ptr(),
                            profiles_file.as_ref().map_or(std::ptr::null(), |path| path.as_ptr()),
                            p_db
                        ))?
                    } else {
                        swss_try!(p_db => SWSSDBConnector_new_keyed(
                            db_name.as_ptr(),
                            timeout_ms,
                            *is_tcp_conn as u8,
                            container_name.as_ptr(),
                            netns.as_ptr(),
                            p_db
                        ))?
                    }
                }
            }
        };

        Ok(Self {
            ptr,
            connection,
            auth_profile,
        })
    }

    /// Create a DbConnector from a named entry in the SONiC db config.
    ///
    /// Timeout of 0 means block indefinitely.
    pub fn new_named(
        db_name: impl Into<String>,
        is_tcp_conn: bool,
        timeout_ms: u32,
    ) -> Result<DbConnector> {
        let db_name = db_name.into();
        Self::new(
            DbConnectionInfo::Named {
                db_name,
                is_tcp_conn,
            },
            timeout_ms,
        )
    }

    pub fn new_named_with_profile(
        db_name: impl Into<String>,
        is_tcp_conn: bool,
        timeout_ms: u32,
        auth_profile: RedisAuthProfile,
    ) -> Result<DbConnector> {
        let db_name = db_name.into();
        Self::new_with_profile(
            DbConnectionInfo::Named {
                db_name,
                is_tcp_conn,
            },
            timeout_ms,
            auth_profile,
        )
    }

    /// Create a DbConnector over a tcp socket.
    ///
    /// Timeout of 0 means block indefinitely.
    pub fn new_tcp(
        db_id: i32,
        hostname: impl Into<String>,
        port: u16,
        timeout_ms: u32,
    ) -> Result<DbConnector> {
        let hostname = hostname.into();
        Self::new(
            DbConnectionInfo::Tcp {
                hostname,
                port,
                db_id,
            },
            timeout_ms,
        )
    }

    pub fn new_tcp_with_profile(
        db_id: i32,
        hostname: impl Into<String>,
        port: u16,
        timeout_ms: u32,
        auth_profile: RedisAuthProfile,
    ) -> Result<DbConnector> {
        let hostname = hostname.into();
        Self::new_with_profile(
            DbConnectionInfo::Tcp {
                hostname,
                port,
                db_id,
            },
            timeout_ms,
            auth_profile,
        )
    }

    /// Create a DbConnector over a unix socket.
    ///
    /// Timeout of 0 means block indefinitely.
    pub fn new_unix(
        db_id: i32,
        sock_path: impl Into<String>,
        timeout_ms: u32,
    ) -> Result<DbConnector> {
        let sock_path = sock_path.into();
        Self::new(DbConnectionInfo::Unix { sock_path, db_id }, timeout_ms)
    }

    pub fn new_unix_with_profile(
        db_id: i32,
        sock_path: impl Into<String>,
        timeout_ms: u32,
        auth_profile: RedisAuthProfile,
    ) -> Result<DbConnector> {
        let sock_path = sock_path.into();
        Self::new_with_profile(
            DbConnectionInfo::Unix { sock_path, db_id },
            timeout_ms,
            auth_profile,
        )
    }

    pub fn new_keyed(
        db_name: impl Into<String>,
        is_tcp_conn: bool,
        timeout_ms: u32,
        container_name: impl Into<String>,
        netns: impl Into<String>,
    ) -> Result<DbConnector> {
        let db_name = db_name.into();
        let container_name = container_name.into();
        let netns = netns.into();
        Self::new(
            DbConnectionInfo::Keyed {
                db_name,
                is_tcp_conn,
                container_name,
                netns,
            },
            timeout_ms,
        )
    }

    pub fn new_keyed_with_profile(
        db_name: impl Into<String>,
        is_tcp_conn: bool,
        timeout_ms: u32,
        container_name: impl Into<String>,
        netns: impl Into<String>,
        auth_profile: RedisAuthProfile,
    ) -> Result<DbConnector> {
        let db_name = db_name.into();
        let container_name = container_name.into();
        let netns = netns.into();
        Self::new_with_profile(
            DbConnectionInfo::Keyed {
                db_name,
                is_tcp_conn,
                container_name,
                netns,
            },
            timeout_ms,
            auth_profile,
        )
    }

    /// Clone a DbConnector with a timeout.
    ///
    /// Timeout of 0 means block indefinitely.
    pub fn clone_timeout(&self, timeout_ms: u32) -> Result<Self> {
        match self.auth_profile.clone() {
            Some(auth_profile) => {
                Self::new_with_profile(self.connection.clone(), timeout_ms, auth_profile)
            }
            None => Self::new(self.connection.clone(), timeout_ms),
        }
    }

    pub fn connection(&self) -> &DbConnectionInfo {
        &self.connection
    }

    pub fn del(&self, key: &str) -> Result<bool> {
        let key = cstr(key)?;
        let status = unsafe {
            swss_try!(p_status => SWSSDBConnector_del(self.ptr, key.as_ptr(), p_status))?
        };
        Ok(status == 1)
    }

    pub fn set(&self, key: &str, val: &CxxStr) -> Result<()> {
        let key = cstr(key)?;
        unsafe { swss_try!(SWSSDBConnector_set(self.ptr, key.as_ptr(), val.as_raw())) }
    }

    pub fn get(&self, key: &str) -> Result<Option<CxxString>> {
        let key = cstr(key)?;
        unsafe {
            let ans = swss_try!(p_ans => SWSSDBConnector_get(self.ptr, key.as_ptr(), p_ans))?;
            Ok(CxxString::take(ans))
        }
    }

    pub fn exists(&self, key: &str) -> Result<bool> {
        let key = cstr(key)?;
        let status = unsafe {
            swss_try!(p_status => SWSSDBConnector_exists(self.ptr, key.as_ptr(), p_status))?
        };
        Ok(status == 1)
    }

    pub fn hdel(&self, key: &str, field: &str) -> Result<bool> {
        let key = cstr(key)?;
        let field = cstr(field)?;
        let status = unsafe {
            swss_try!(p_status => SWSSDBConnector_hdel(self.ptr, key.as_ptr(), field.as_ptr(), p_status))?
        };
        Ok(status == 1)
    }

    pub fn hset(&self, key: &str, field: &str, val: &CxxStr) -> Result<()> {
        let key = cstr(key)?;
        let field = cstr(field)?;
        unsafe {
            swss_try!(SWSSDBConnector_hset(
                self.ptr,
                key.as_ptr(),
                field.as_ptr(),
                val.as_raw(),
            ))
        }
    }

    pub fn hget(&self, key: &str, field: &str) -> Result<Option<CxxString>> {
        let key = cstr(key)?;
        let field = cstr(field)?;
        unsafe {
            let ans = swss_try!(p_ans => SWSSDBConnector_hget(self.ptr, key.as_ptr(), field.as_ptr(), p_ans))?;
            Ok(CxxString::take(ans))
        }
    }

    pub fn hgetall(&self, key: &str) -> Result<HashMap<String, CxxString>> {
        let key = cstr(key)?;
        unsafe {
            let arr = swss_try!(p_arr => SWSSDBConnector_hgetall(self.ptr, key.as_ptr(), p_arr))?;
            take_field_value_array(arr)
        }
    }

    pub fn hexists(&self, key: &str, field: &str) -> Result<bool> {
        let key = cstr(key)?;
        let field = cstr(field)?;
        let status = unsafe {
            swss_try!(p_status => SWSSDBConnector_hexists(self.ptr, key.as_ptr(), field.as_ptr(), p_status))?
        };
        Ok(status == 1)
    }

    pub fn flush_db(&self) -> Result<bool> {
        let status = unsafe { swss_try!(p_status => SWSSDBConnector_flushdb(self.ptr, p_status))? };
        Ok(status == 1)
    }
}

impl Drop for DbConnector {
    fn drop(&mut self) {
        unsafe {
            if let Err(e) = swss_try!(SWSSDBConnector_free(self.ptr)) {
                eprintln!("Error dropping DbConnector: {}", e);
            }
        }
    }
}

unsafe impl Send for DbConnector {}

#[cfg(feature = "async")]
impl DbConnector {
    async_util::impl_basic_async_method!(new_async <= new(connection: DbConnectionInfo, timeout_ms: u32) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(new_with_profile_async <= new_with_profile(connection: DbConnectionInfo, timeout_ms: u32, auth_profile: RedisAuthProfile) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(new_named_async <= new_named(db_name: &str, is_tcp_conn: bool, timeout_ms: u32) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(new_named_with_profile_async <= new_named_with_profile(db_name: &str, is_tcp_conn: bool, timeout_ms: u32, auth_profile: RedisAuthProfile) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(new_tcp_async <= new_tcp(db_id: i32, hostname: &str, port: u16, timeout_ms: u32) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(new_tcp_with_profile_async <= new_tcp_with_profile(db_id: i32, hostname: &str, port: u16, timeout_ms: u32, auth_profile: RedisAuthProfile) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(new_unix_async <= new_unix(db_id: i32, sock_path: &str, timeout_ms: u32) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(new_unix_with_profile_async <= new_unix_with_profile(db_id: i32, sock_path: &str, timeout_ms: u32, auth_profile: RedisAuthProfile) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(new_keyed_async <= new_keyed(db_name: &str, is_tcp_conn: bool, timeout_ms: u32, container_name: &str, netns: &str) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(new_keyed_with_profile_async <= new_keyed_with_profile(db_name: &str, is_tcp_conn: bool, timeout_ms: u32, container_name: &str, netns: &str, auth_profile: RedisAuthProfile) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(clone_timeout_async <= clone_timeout(&self, timeout_ms: u32) -> Result<DbConnector>);
    async_util::impl_basic_async_method!(del_async <= del(&self, key: &str) -> Result<bool>);
    async_util::impl_basic_async_method!(set_async <= set(&self, key: &str, value: &CxxStr) -> Result<()>);
    async_util::impl_basic_async_method!(get_async <= get(&self, key: &str) -> Result<Option<CxxString>>);
    async_util::impl_basic_async_method!(exists_async <= exists(&self, key: &str) -> Result<bool>);
    async_util::impl_basic_async_method!(hdel_async <= hdel(&self, key: &str, field: &str) -> Result<bool>);
    async_util::impl_basic_async_method!(hset_async <= hset(&self, key: &str, field: &str, value: &CxxStr) -> Result<()>);
    async_util::impl_basic_async_method!(hget_async <= hget(&self, key: &str, field: &str) -> Result<Option<CxxString>>);
    async_util::impl_basic_async_method!(hgetall_async <= hgetall(&self, key: &str) -> Result<HashMap<String, CxxString>>);
    async_util::impl_basic_async_method!(hexists_async <= hexists(&self, key: &str, field: &str) -> Result<bool>);
    async_util::impl_basic_async_method!(flush_db_async <= flush_db(&self) -> Result<bool>);
}
