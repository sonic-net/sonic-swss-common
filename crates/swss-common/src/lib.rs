mod bindings {
    #![allow(unused, non_snake_case, non_upper_case_globals, non_camel_case_types)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}
mod types;

pub use types::*;

/// Rust wrapper around `swss::SonicDBConfig::initialize`.
pub fn sonic_db_config_initialize(path: &str) -> Result<(), Exception> {
    let path = cstr(path);
    unsafe { swss_try!(bindings::SWSSSonicDBConfig_initialize(path.as_ptr())) }
}

/// Rust wrapper around `swss::SonicDBConfig::initializeGlobalConfig`.
pub fn sonic_db_config_initialize_global(path: &str) -> Result<(), Exception> {
    let path = cstr(path);
    unsafe { swss_try!(bindings::SWSSSonicDBConfig_initializeGlobalConfig(path.as_ptr())) }
}
