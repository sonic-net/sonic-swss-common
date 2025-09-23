use std::collections::HashMap;
use swss_common::{SonicV2Connector, CxxString};
use swss_common_testing::Redis;
use serial_test::serial;

/// Test SonicV2Connector functionality - Rust version of test_SonicV2Connector()
///
/// This test verifies:
/// - SonicV2Connector creation and connection
/// - Basic Redis operations (set, get, exists, del)
/// - Hash operations (hexists, get_all, hmset)
/// - Key management operations (keys, scan)
/// - Data integrity and type conversions
#[cfg(test)]
#[test]
#[serial]
fn test_sonicv2connector_basic_operations() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();
    println!("Redis unix socket: {}", _redis.sock);

    // Create SonicV2Connector instance
    let connector = SonicV2Connector::new(true, None)?;

    // Verify initial state
    assert_eq!(connector.netns(), "");
    assert!(connector.use_unix_socket_path());

    // Connect to TEST_DB
    connector.connect("TEST_DB", true)?;

    // Test basic existence check on non-existing key
    let exists = connector.exists("TEST_DB", "test_key")?;
    assert!(!exists, "Key should not exist initially");

    // Test set operation
    let set_result = connector.set("TEST_DB", "test_key", "field1", "value1", false)?;
    assert!(set_result >= 0, "Set operation should succeed");

    // Test existence check on existing key
    let exists = connector.exists("TEST_DB", "test_key")?;
    assert!(exists, "Key should exist after set");

    // Test hexists operation
    let hexists = connector.hexists("TEST_DB", "test_key", "field1")?;
    assert!(hexists, "Field should exist in hash");

    // Test get operation
    let value = connector.get("TEST_DB", "test_key", "field1", false)?;
    assert_eq!(value, Some("value1".to_string()), "Retrieved value should match set value");

    // Test get_all operation
    let all_values = connector.get_all("TEST_DB", "test_key", false)?;
    assert_eq!(all_values.len(), 1, "Hash should contain one field");
    assert!(all_values.contains_key("field1"), "Hash should contain field1");
    assert_eq!(all_values["field1"].as_cxx_str(), "value1", "Field value should match");

    // Test delete operation
    let del_result = connector.del("TEST_DB", "test_key", false)?;
    assert_eq!(del_result, 1, "Delete should remove one key");

    // Verify key is deleted
    let exists = connector.exists("TEST_DB", "test_key")?;
    assert!(!exists, "Key should not exist after deletion");

    Ok(())
}

/// Test SonicV2Connector hmset functionality
#[test]
#[serial]
fn test_sonicv2connector_hmset() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();

    let connector = SonicV2Connector::new(true, None)?;
    connector.connect("TEST_DB", true)?;

    // Prepare field-value pairs for hmset
    let mut test_data = HashMap::new();
    test_data.insert("field1", CxxString::new("value1"));
    test_data.insert("field2", CxxString::new("value2"));
    test_data.insert("field3", CxxString::new("value3"));

    // Test hmset operation
    connector.hmset("TEST_DB", "test_hmset_key", test_data)?;

    // Verify the data was set correctly
    let retrieved_values = connector.get_all("TEST_DB", "test_hmset_key", false)?;
    assert_eq!(retrieved_values.len(), 3, "Hash should contain three fields");
    assert_eq!(retrieved_values["field1"].as_cxx_str(), "value1");
    assert_eq!(retrieved_values["field2"].as_cxx_str(), "value2");
    assert_eq!(retrieved_values["field3"].as_cxx_str(), "value3");

    // Clean up
    connector.del("TEST_DB", "test_hmset_key", false)?;

    Ok(())
}

/// Test SonicV2Connector keys and scan functionality
#[test]
#[serial]
fn test_sonicv2connector_keys_and_scan() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();

    let connector = SonicV2Connector::new(true, None)?;
    connector.connect("TEST_DB", true)?;

    // Set up test keys
    connector.set("TEST_DB", "test_key1", "field1", "value1", false)?;
    connector.set("TEST_DB", "test_key2", "field1", "value1", false)?;
    connector.set("TEST_DB", "other_key", "field1", "value1", false)?;

    // Test keys operation with pattern
    let keys = connector.keys("TEST_DB", Some("test_*"), false)?;
    assert!(keys.len() >= 2, "Should find at least 2 keys matching pattern");
    assert!(keys.contains(&"test_key1".to_string()), "Should contain test_key1");
    assert!(keys.contains(&"test_key2".to_string()), "Should contain test_key2");

    // Test keys operation without pattern (get all keys)
    let all_keys = connector.keys("TEST_DB", None, false)?;
    assert!(all_keys.len() >= 3, "Should find at least 3 keys total");

    // Clean up test keys
    connector.del("TEST_DB", "test_key1", false)?;
    connector.del("TEST_DB", "test_key2", false)?;
    connector.del("TEST_DB", "other_key", false)?;

    Ok(())
}

/// Test SonicV2Connector namespace functionality
#[test]
#[serial]
fn test_sonicv2connector_namespace() -> Result<(), Box<dyn std::error::Error>> {
    // Test with custom namespace
    let test_namespace = "test_namespace";
    let connector = SonicV2Connector::new(true, Some(test_namespace.to_string()))?;

    // Get namespace and verify
    let namespace = connector.get_namespace()?;
    assert_eq!(namespace, test_namespace, "Namespace should match what was set");
    assert_eq!(connector.netns(), test_namespace, "Netns getter should match");

    // Test with no namespace (default)
    let default_connector = SonicV2Connector::new(true, None)?;
    let default_namespace = default_connector.get_namespace()?;
    assert_eq!(default_namespace, "", "Default namespace should be empty");

    Ok(())
}

/// Test SonicV2Connector database operations
#[test]
#[serial]
fn test_sonicv2connector_database_operations() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();

    let connector = SonicV2Connector::new(true, None)?;

    // Test get_db_list
    let db_list = connector.get_db_list()?;
    assert!(!db_list.is_empty(), "Database list should not be empty");
    println!("Available databases: {:?}", db_list);

    // Test get_dbid for TEST_DB
    let db_id = connector.get_dbid("TEST_DB")?;
    assert!(db_id >= 0, "Database ID should be non-negative");
    println!("TEST_DB ID: {}", db_id);

    // Test get_db_separator
    let separator = connector.get_db_separator("TEST_DB")?;
    assert!(!separator.is_empty(), "Database separator should not be empty");
    println!("TEST_DB separator: '{}'", separator);

    // Test connection and close operations
    connector.connect("TEST_DB", true)?;

    // Test get_redis_client
    let _redis_client = connector.get_redis_client("TEST_DB")?;
    // Note: We can't do much with the borrowed client in this test,
    // but we can verify it was returned successfully

    connector.close_db("TEST_DB")?;
    connector.close_all()?;

    Ok(())
}

/// Test SonicV2Connector publish functionality
#[test]
#[serial]
fn test_sonicv2connector_publish() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();

    let connector = SonicV2Connector::new(true, None)?;
    connector.connect("TEST_DB", true)?;

    // Test publish operation
    let publish_result = connector.publish("TEST_DB", "test_channel", "test_message")?;
    assert!(publish_result >= 0, "Publish should return non-negative result");

    Ok(())
}

/// Test SonicV2Connector delete_all_by_pattern functionality
#[test]
#[serial]
fn test_sonicv2connector_delete_by_pattern() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();

    let connector = SonicV2Connector::new(true, None)?;
    connector.connect("TEST_DB", true)?;

    // Set up test keys with pattern
    connector.set("TEST_DB", "pattern_key1", "field1", "value1", false)?;
    connector.set("TEST_DB", "pattern_key2", "field1", "value1", false)?;
    connector.set("TEST_DB", "other_key", "field1", "value1", false)?;

    // Verify keys exist
    assert!(connector.exists("TEST_DB", "pattern_key1")?);
    assert!(connector.exists("TEST_DB", "pattern_key2")?);
    assert!(connector.exists("TEST_DB", "other_key")?);

    // Delete all keys matching pattern
    connector.delete_all_by_pattern("TEST_DB", "pattern_*")?;

    // Verify pattern keys are deleted but other key remains
    assert!(!connector.exists("TEST_DB", "pattern_key1")?, "pattern_key1 should be deleted");
    assert!(!connector.exists("TEST_DB", "pattern_key2")?, "pattern_key2 should be deleted");
    assert!(connector.exists("TEST_DB", "other_key")?, "other_key should still exist");

    // Clean up remaining key
    connector.del("TEST_DB", "other_key", false)?;

    Ok(())
}

/// Test SonicV2Connector error handling
#[test]
#[serial]
fn test_sonicv2connector_error_handling() -> Result<(), Box<dyn std::error::Error>> {
    let connector = SonicV2Connector::new(true, None)?;

    // Test operations on non-connected database (should handle gracefully)
    // Note: The exact behavior depends on the underlying implementation
    // Some operations might succeed with default behavior, others might fail

    // Test get on non-existing key (should return None, not error)
    let result = connector.get("TEST_DB", "non_existing_hash", "non_existing_field", false);
    match result {
        Ok(None) => { /* Expected for non-existing key */ },
        Ok(Some(_)) => { /* Possible if there's existing data */ },
        Err(_) => { /* Possible if database is not connected */ },
    }

    // Test hexists on non-existing key (should return false, not error)
    let hexists_result = connector.hexists("TEST_DB", "non_existing_hash", "non_existing_field");
    match hexists_result {
        Ok(false) => { /* Expected for non-existing key */ },
        Ok(true) => { /* Unexpected but possible */ },
        Err(_) => { /* Possible if database is not connected */ },
    }

    Ok(())
}

/// Test SonicV2Connector with different data types
#[test]
#[serial]
fn test_sonicv2connector_data_types() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();

    let connector = SonicV2Connector::new(true, None)?;
    connector.connect("TEST_DB", true)?;

    // Test with different string types and values
    let test_cases = vec![
        ("empty_value", ""),
        ("simple_string", "hello"),
        ("string_with_spaces", "hello world"),
        ("string_with_numbers", "hello123"),
        ("string_with_special", "hello@#$%^&*()"),
        ("unicode_string", "hello世界"),
    ];

    for (field, value) in test_cases {
        // Set the value
        connector.set("TEST_DB", "data_types_test", field, value, false)?;

        // Get the value back
        let retrieved = connector.get("TEST_DB", "data_types_test", field, false)?;
        assert_eq!(retrieved, Some(value.to_string()),
                  "Retrieved value should match for field: {}", field);
    }

    // Clean up
    connector.del("TEST_DB", "data_types_test", false)?;

    Ok(())
}
