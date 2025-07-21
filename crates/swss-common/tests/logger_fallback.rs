use swss_common::*;

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
    let handler = LoggerConfigHandlerForTest {};
    // connect to swsscommon logger
    let result = link_to_swsscommon_logger("test", handler);

    assert!(result.is_err());
}
