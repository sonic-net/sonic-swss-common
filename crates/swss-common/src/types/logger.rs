use super::*;
use getset::{Getters, Setters};
use lazy_static::lazy_static;
use std::ffi::c_char;
use std::sync::Mutex;

lazy_static! {
    static ref LOGGER: Mutex<Logger> = Mutex::new(Logger::default());
}

pub trait LoggerConfigChangeHandler: Send {
    fn on_log_level_change(&mut self, level: &str);
    fn on_log_output_change(&mut self, output: &str);
}

#[derive(Getters, Setters)]
pub struct Logger {
    #[getset(get = "pub", set)]
    level: String,
    #[getset(get = "pub", set)]
    output: String,
    #[getset(skip)]
    handler: Option<Box<dyn LoggerConfigChangeHandler>>,
}

impl Default for Logger {
    fn default() -> Self {
        Self {
            level: "INFO".to_string(),
            output: "STDOUT".to_string(),
            handler: None,
        }
    }
}

pub fn link_to_swsscommon_logger<T>(db_name: &str, logger_change_handler: T) -> Result<()>
where
    T: LoggerConfigChangeHandler + 'static,
{
    {
        let mut logger = LOGGER.lock().unwrap();
        logger.handler = Some(Box::new(logger_change_handler));
    }

    let db_name = cstr(db_name);
    let def_level = cstr("INFO");
    let def_output = cstr("SYSLOG");
    // link to config db using the specified db_name. It will trigger immediate
    // callback to set the current log level and output
    unsafe {
        swss_try!(SWSSLogger_linkToDbWithOutput(
            db_name.as_ptr(),
            Some(priority_change_notify),
            def_level.as_ptr(),
            Some(output_change_notify),
            def_output.as_ptr(),
        ))?;
        // Start logger thread to handle log update from config db
        swss_try!(SWSSLogger_restartLogger())?;
        Ok(())
    }
}

extern "C" fn priority_change_notify(_component: *const c_char, priority: *const c_char) {
    let priority = unsafe { CStr::from_ptr(priority) };
    let priority = priority.to_str().unwrap();

    let mut logger = LOGGER.lock().unwrap();
    logger.handler.as_mut().unwrap().as_mut().on_log_level_change(priority);

    logger.set_level(priority.to_string());

    println!("Priority set to: {priority}");
}

extern "C" fn output_change_notify(_component: *const c_char, output: *const c_char) {
    let output = unsafe { CStr::from_ptr(output) };
    let output = output.to_str().unwrap();

    let mut logger = LOGGER.lock().unwrap();
    logger.handler.as_mut().unwrap().as_mut().on_log_output_change(output);
    logger.set_output(output.to_string());
    println!("Output set to: {output}");
}

pub fn log_level() -> String {
    LOGGER.lock().unwrap().level().to_string()
}

pub fn log_output() -> String {
    LOGGER.lock().unwrap().output().to_string()
}
