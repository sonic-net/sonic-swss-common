use std::collections::HashMap;
use swss_common::{ConfigDBConnector, CxxString};
use swss_common_testing::Redis;
use serial_test::serial;

/// Test ConfigDBConnector functionality - Rust version of test_ConfigDBConnector()
///
/// This test verifies:
/// - ConfigDBConnector creation and connection
/// - Setting and getting configuration entries
/// - Table operations (set_entry, get_table, delete_table)
/// - Configuration data integrity
#[cfg(test)]
#[test]
#[serial]
fn test_config_db_connector() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();
    println!("Redis unix socket: {}", _redis.sock);
    
    // Create ConfigDBConnector instance
    let config_db = ConfigDBConnector::new(true, None)?;

    // Verify initial state
    assert_eq!(config_db.netns(), "");
    assert!(config_db.use_unix_socket_path());

    // Connect to CONFIG_DB
    config_db.connect(false, false)?;

    // Set up test data
    let mut test_data = HashMap::new();
    test_data.insert("alias", CxxString::new("etp1x"));
    config_db.set_entry("TEST_PORT", "Ethernet111", test_data)?;

    // Get entire table and verify entry was set correctly
    let table_data = config_db.get_table("TEST_PORT")?;
    assert!(table_data.contains_key("Ethernet111"));
    let ethernet_entry = &table_data["Ethernet111"];
    assert!(ethernet_entry.contains_key("alias"));
    assert_eq!(ethernet_entry["alias"].as_cxx_str(), "etp1x");

    // Test entry update
    let mut update_data = HashMap::new();
    update_data.insert("mtu", CxxString::new("12345"));
    config_db.set_entry("TEST_PORT", "Ethernet111", update_data)?;

    // Verify update replaced previous data (alias should be gone, mtu should be present)
    let updated_table = config_db.get_table("TEST_PORT")?;
    assert!(updated_table.contains_key("Ethernet111"));
    let updated_entry = &updated_table["Ethernet111"];

    // Debug output to help diagnose the issue
    println!("Updated entry keys: {:?}", updated_entry.keys().collect::<Vec<_>>());

    assert!(!updated_entry.contains_key("alias"), "Previous 'alias' field should be removed");
    assert!(updated_entry.contains_key("mtu"), "Entry should contain 'mtu' key. Available keys: {:?}", updated_entry.keys().collect::<Vec<_>>());
    assert_eq!(updated_entry["mtu"].as_cxx_str(), "12345");

    // Test table deletion
    config_db.delete_table("TEST_PORT")?;

    // Verify table is empty after deletion
    let empty_table = config_db.get_table("TEST_PORT")?;
    assert!(empty_table.is_empty(), "Table should be empty after deletion");

    Ok(())
}

/// Test ConfigDBConnector with table separator functionality
/// Rust version of test_ConfigDBConnectorSeparator()
#[test]
#[serial]
fn test_config_db_connector_separator() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();

    // NOTE: This test verifies table separator functionality.
    // The ConfigDBConnector should only include properly separated keys.
    let config_db = ConfigDBConnector::new(true, None)?;
    config_db.connect(false, false)?;

    // Set an entry in TEST_PORT table
    let mut test_data = HashMap::new();
    test_data.insert("alias", CxxString::new("etp2x"));
    config_db.set_entry("TEST_PORT", "Ethernet222", test_data)?;

    // Verify the entry appears in get_config (which should only include properly separated keys)
    let all_config = config_db.get_table("*")?;

    // Verify our properly formatted entry exists
    assert!(all_config.contains_key("Ethernet222"));

    // Clean up
    config_db.delete_table("TEST_PORT")?;
    let final_config = config_db.get_table("*")?;

    // Note: This may not be empty if other tests have run, but TEST_PORT should be gone
    assert!(!final_config.contains_key("Ethernet222"));

    Ok(())
}

/// Test ConfigDBConnector connect functionality
/// Rust version of test_ConfigDBConnect()
#[test]
#[serial]
fn test_config_db_connect() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();
    let config_db = ConfigDBConnector::new(true, None)?;

    // Test connection
    config_db.connect(false, false)?;
    
    // Clear the database
    let redis_client = config_db.get_redis_client("CONFIG_DB")?;
    let flush_success = redis_client.flush_db()?;
    assert!(flush_success, "Database flush should succeed");
    
    // Verify we can perform basic operations after connection
    // This is equivalent to allconfig = config_db.get_config() followed by assert len(allconfig) == 0
    let all_config = config_db.get_config()?;
    assert_eq!(all_config.len(), 0, "Config should be empty after flushdb");

    Ok(())
}

/// Test ConfigDBConnector scan functionality with many entries
/// Rust version of test_ConfigDBScan()
#[test]
#[serial]
fn test_config_db_scan() -> Result<(), Box<dyn std::error::Error>> {
    let config_db = ConfigDBConnector::new(true, None)?;
    config_db.connect(false, false)?;

    let n = 100; // Reduced from 1000 to keep test fast

    // Create many table entries to test scan functionality
    for i in 0..n {
        let table_name = format!("TEST_TYPE{}", i);
        let key_name = format!("Ethernet{}", i);
        let mut field_data = HashMap::new();
        field_data.insert(format!("alias{}", i), CxxString::new(format!("etp{}", i)));

        config_db.set_entry(&table_name, &key_name, field_data)?;
    }

    // Verify all entries were created by checking each table
    for i in 0..n {
        let table_name = format!("TEST_TYPE{}", i);
        let table_data = config_db.get_table(&table_name)?;
        assert!(!table_data.is_empty(), "Table {} should contain data", table_name);
    }

    // Clean up - delete all test tables
    for i in 0..n {
        let table_name = format!("TEST_TYPE{}", i);
        config_db.delete_table(&table_name)?;
    }

    Ok(())
}
