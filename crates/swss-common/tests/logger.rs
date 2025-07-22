use lazy_static::lazy_static;
use std::sync::Mutex;
use std::thread::sleep;
use std::time::Duration;
use swss_common::*;
use swss_common_testing::*;

lazy_static! {
    static ref LEVEL: Mutex<String> = Mutex::new("INFO".to_string());
    static ref OUTPUT: Mutex<String> = Mutex::new("INFO".to_string());
}
struct LoggerConfigHandlerForTest {}

impl LoggerConfigChangeHandler for LoggerConfigHandlerForTest {
    fn on_log_level_change(&mut self, level_str: &str) {
        let mut level = LEVEL.lock().unwrap();
        *level = level_str.to_string();
    }

    fn on_log_output_change(&mut self, output_str: &str) {
        let mut output = OUTPUT.lock().unwrap();
        *output = output_str.to_string();
    }
}
#[test]
fn logger_init_test() -> Result<(), Exception> {
    let redis = Redis::start_config_db();
    // todo: change redis::db_connector to passing db_id as parameter
    let db = DbConnector::new_unix(1, &redis.sock, 0)?;

    db.hset("LOGGER|test", "LOGLEVEL", &CxxString::new("NOTICE"))?;
    db.hset("LOGGER|test", "LOGOUTPUT", &CxxString::new("SYSLOG"))?;

    // connect to swsscommon logger
    let handler = LoggerConfigHandlerForTest {};
    link_to_swsscommon_logger("test", handler)?;

    assert_eq!("NOTICE", *LEVEL.lock().unwrap());
    assert_eq!("SYSLOG", *OUTPUT.lock().unwrap());

    // test log level change
    db.hset("LOGGER|test", "LOGLEVEL", &CxxString::new("CRIT"))?;
    db.hset("LOGGER|test", "LOGOUTPUT", &CxxString::new("STDOUT"))?;
    sleep(Duration::from_millis(500));

    assert_eq!("CRIT", *LEVEL.lock().unwrap());
    assert_eq!("STDOUT", *OUTPUT.lock().unwrap());
    Ok(())
}
