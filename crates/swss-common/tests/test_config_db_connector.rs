use std::collections::HashMap;
use swss_common::{ConfigDBConnector, CxxString};

/// Test ConfigDBConnector functionality - Rust version of test_ConfigDBConnector()
/// 
/// This test verifies:
/// - ConfigDBConnector creation and connection
/// - Setting and getting configuration entries  
/// - Table operations (set_entry, get_table, delete_table)
/// - Configuration data integrity
#[cfg(test)]
#[test]
fn test_config_db_connector() -> Result<(), Box<dyn std::error::Error>> {
    // Create ConfigDBConnector instance
    let config_db = ConfigDBConnector::new(true, None)?;
    
    // Verify initial state
    assert_eq!(config_db.netns(), "");
    assert!(config_db.use_unix_socket_path());
    
    // Connect to CONFIG_DB (equivalent to Python's connect(wait_for_init=False))
    config_db.connect(false, false)?;
    
    // Set up test data - equivalent to Python's set_entry("TEST_PORT", "Ethernet111", {"alias": "etp1x"})
    let mut test_data = HashMap::new();
    test_data.insert("alias", CxxString::new("etp1x"));
    config_db.set_entry("TEST_PORT", "Ethernet111", test_data)?;
    
    // Get entire table and verify entry was set correctly
    let table_data = config_db.get_table("TEST_PORT")?;
    assert!(table_data.contains_key("Ethernet111"));
    let ethernet_entry = &table_data["Ethernet111"];
    assert!(ethernet_entry.contains_key("alias"));
    assert_eq!(ethernet_entry["alias"].as_cxx_str(), "etp1x");
    
    // Test entry update - equivalent to Python's set_entry("TEST_PORT", "Ethernet111", {"mtu": "12345"})
    let mut update_data = HashMap::new();
    update_data.insert("mtu", CxxString::new("12345"));
    config_db.set_entry("TEST_PORT", "Ethernet111", update_data)?;
    
    // Verify update replaced previous data (alias should be gone, mtu should be present)
    let updated_table = config_db.get_table("TEST_PORT")?;
    let updated_entry = &updated_table["Ethernet111"];
    assert!(!updated_entry.contains_key("alias"), "Previous 'alias' field should be removed");
    assert!(updated_entry.contains_key("mtu"));
    assert_eq!(updated_entry["mtu"].as_cxx_str(), "12345");
    
    // Test table deletion
    config_db.delete_table("TEST_PORT")?;
    
    // Verify table is empty after deletion
    let empty_table = config_db.get_table("TEST_PORT")?;
    assert!(empty_table.is_empty(), "Table should be empty after deletion");
    
    Ok(())
}
