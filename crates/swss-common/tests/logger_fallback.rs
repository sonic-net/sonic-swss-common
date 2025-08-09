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
    // Print current process list
    let ps_output = Command::new("ps")
        .args(&["aux"])
        .output();
    assert!(ps_output.is_ok(), "Failed to execute ps command");
    let ps_result = ps_output.unwrap();
    assert!(ps_result.status.success(), "ps command failed");
    println!("Process list:\n{}", String::from_utf8_lossy(&ps_result.stdout));

    let handler = LoggerConfigHandlerForTest {};
    // connect to swsscommon logger - should fail when Redis is not available
    let result = link_to_swsscommon_logger("test", handler);

    assert!(result.is_err());
}
