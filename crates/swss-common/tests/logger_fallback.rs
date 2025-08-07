use swss_common::*;
use std::process::Command;

struct LoggerConfigHandlerForTest {}

impl LoggerConfigChangeHandler for LoggerConfigHandlerForTest {
    #[allow(unused_variables)]
    fn on_log_level_change(&mut self, level_str: &str) {}

    #[allow(unused_variables)]
    fn on_log_output_change(&mut self, output_str: &str) {}
}

// Test link_to_swsscommon_logger returning an error when Redis is not available.
// This test has to be run in a separate test suite because it can't run with other tests that use Redis.
#[test]
fn logger_init_without_redis() {
    // Stop redis-server service to ensure it's not available
    let _ = Command::new("sudo")
        .args(&["service", "redis-server", "stop"])
        .status();

    // Give some time for the service to stop
    std::thread::sleep(std::time::Duration::from_millis(500));

    let handler = LoggerConfigHandlerForTest {};
    // connect to swsscommon logger - should fail when Redis is not available
    let result = link_to_swsscommon_logger("test", handler);

    assert!(result.is_err());

    // Restart redis-server service
    let _ = Command::new("sudo")
        .args(&["service", "redis-server", "start"])
        .status();

    // Give some time for the service to start
    std::thread::sleep(std::time::Duration::from_millis(1000));
}
