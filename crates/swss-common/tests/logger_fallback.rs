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
    let stop_result = Command::new("sudo")
        .args(&["service", "redis-server", "stop"])
        .status();

    // Assert the stop command succeeded
    assert!(stop_result.is_ok(), "Failed to execute stop command");
    let stop_status = stop_result.unwrap();
    assert!(stop_status.success(), "Stop command failed with exit code: {:?}", stop_status.code());

    // Print current process list after stopping redis
    let ps_output = Command::new("ps")
        .args(&["aux"])
        .output();
    assert!(ps_output.is_ok(), "Failed to execute ps command");
    let ps_result = ps_output.unwrap();
    assert!(ps_result.status.success(), "ps command failed");
    println!("Process list after stopping redis:\n{}", String::from_utf8_lossy(&ps_result.stdout));

    // Give some time for the service to stop
    std::thread::sleep(std::time::Duration::from_millis(500));

    let handler = LoggerConfigHandlerForTest {};
    // connect to swsscommon logger - should fail when Redis is not available
    let result = link_to_swsscommon_logger("test", handler);

    assert!(result.is_err());

    // Restart redis-server service
    let start_result = Command::new("sudo")
        .args(&["service", "redis-server", "start"])
        .status();

    // Assert the start command succeeded
    assert!(start_result.is_ok(), "Failed to execute start command");
    let start_status = start_result.unwrap();
    assert!(start_status.success(), "Start command failed with exit code: {:?}", start_status.code());

    // Print current process list after starting redis
    let ps_output_after = Command::new("ps")
        .args(&["aux"])
        .output();
    assert!(ps_output_after.is_ok(), "Failed to execute ps command after restart");
    let ps_result_after = ps_output_after.unwrap();
    assert!(ps_result_after.status.success(), "ps command failed after restart");
    println!("Process list after restarting redis:\n{}", String::from_utf8_lossy(&ps_result_after.stdout));

    // Give some time for the service to start
    std::thread::sleep(std::time::Duration::from_millis(1000));
}
