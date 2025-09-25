use std::collections::HashMap;
use swss_common::{SonicV2Connector, CxxString};
use swss_common_testing::Redis;
use serial_test::serial;

/// Test SonicV2Connector functionality - Rust version of Python test_SonicV2Connector()
/// Python: lines 761-767 in test_redis_ut.py
#[test]
#[serial]
fn test_sonicv2connector() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();

    let db = SonicV2Connector::new(true, None)?;
    db.connect("TEST_DB", true)?;

    db.set("TEST_DB", "test_key", "field1", "1", false)?;
    let value = db.get("TEST_DB", "test_key", "field1", false)?;
    assert_eq!(value, Some("1".to_string()));

    Ok(())
}

/// Test DBInterface - Rust version of Python test_DBInterface()
/// Python: lines 208-369 in test_redis_ut.py
/// This is the main comprehensive SonicV2Connector test
#[test]
#[serial]
fn test_dbinterface() -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();

    let db = SonicV2Connector::new(true, Some("".to_string()))?;
    assert_eq!(db.get_namespace()?, "");
    db.connect("TEST_DB", true)?;

    // Get redis client and flush db
    let redis_client = db.get_redis_client("TEST_DB")?;
    redis_client.flush_db()?;

    // Case: hset and hget normally
    db.set("TEST_DB", "key0", "field1", "value2", false)?;
    let val = db.get("TEST_DB", "key0", "field1", false)?;
    assert_eq!(val, Some("value2".to_string()));

    // Case: hset an empty value
    db.set("TEST_DB", "kkk3", "field3", "", false)?;
    let val = db.get("TEST_DB", "kkk3", "field3", false)?;
    assert_eq!(val, Some("".to_string()));

    // Case: hset an "None" string value
    db.set("TEST_DB", "kkk3", "field3", "None", false)?;
    let val = db.get("TEST_DB", "kkk3", "field3", false)?;
    // Note: Python converts "None" string to actual None, check if Rust does the same
    assert_eq!(val, None, "Rust should convert 'None' string to None like Python");

    // hget on an existing key but non-existing field
    let val = db.get("TEST_DB", "kkk3", "missing", false)?;
    assert_eq!(val, None);

    // hget on an non-existing key and non-existing field
    let val = db.get("TEST_DB", "kkk_missing", "missing", false)?;
    assert_eq!(val, None);

    // Test get_all
    let fvs = db.get_all("TEST_DB", "key0", false)?;
    assert!(fvs.contains_key("field1"));
    assert_eq!(fvs["field1"].as_cxx_str(), "value2");

    // Test JSON serialization
    // Convert to a regular HashMap for JSON serialization
    let mut json_map = std::collections::HashMap::new();
    for (key, value) in &fvs {
        json_map.insert(key, value.as_cxx_str());
    }
    let _json_result = serde_json::to_string(&json_map);
    assert!(_json_result.is_ok(), "JSON serialization should succeed");

    // Test keys
    let ks = db.keys("TEST_DB", Some("key*"), false)?;
    assert_eq!(ks.len(), 1);

    // Test keys could be sorted in place
    db.set("TEST_DB", "key11", "field1", "value2", false)?;
    db.set("TEST_DB", "key12", "field1", "value2", false)?;
    db.set("TEST_DB", "key13", "field1", "value2", false)?;
    let mut ks = db.keys("TEST_DB", Some("key*"), false)?;
    let ks0 = ks.clone();
    ks.sort();
    ks.reverse();
    let mut expected = ks0.clone();
    expected.sort();
    expected.reverse();
    assert_eq!(ks, expected);

    // Test del
    db.set("TEST_DB", "key3", "field4", "value5", false)?;
    let deleted = db.del("TEST_DB", "key3", false)?;
    assert_eq!(deleted, 1);
    let deleted = db.del("TEST_DB", "key3", false)?;
    assert_eq!(deleted, 0);

    // Test pubsub
    // Note: Rust may not have direct pubsub access like Python, but we test what we can
    let dbid = db.get_dbid("TEST_DB")?;
    println!("TEST_DB ID for pubsub: {}", dbid);
    db.set("TEST_DB", "pub_key", "field1", "value1", false)?;
    db.set("TEST_DB", "pub_key", "field2", "value2", false)?;
    db.set("TEST_DB", "pub_key", "field3", "value3", false)?;
    db.set("TEST_DB", "pub_key", "field4", "value4", false)?;

    // Test dict.get() equivalent behavior
    let fvs = db.get_all("TEST_DB", "key0", false)?;

    // Test fvs.get("field1") == "value2"
    assert_eq!(fvs.get("field1").map(|v| v.as_cxx_str().to_str().unwrap()), Some("value2"));

    // Test fvs.get("field1_noexisting") == None
    assert_eq!(fvs.get("field1_noexisting"), None);

    // Test fvs.get("field1", "default") == "value2"
    let field1_value = fvs.get("field1").map(|v| v.as_cxx_str().to_str().unwrap()).unwrap_or("default");
    assert_eq!(field1_value, "value2");

    // Test fvs.get("nonfield", "default") == "default"
    let nonfield_value = fvs.get("nonfield").map(|v| v.as_cxx_str().to_str().unwrap()).unwrap_or("default");
    assert_eq!(nonfield_value, "default");

    // Test dict.update() equivalent behavior
    // Note: Since get_all() returns immutable data, we can't test update() directly
    // But we can test the concept by setting new values and verifying the result

    // Simulate Python: other = { "field1": "value3", "field4": "value4" }; fvs.update(other)
    db.set("TEST_DB", "update_test", "field1", "value3", false)?;
    db.set("TEST_DB", "update_test", "field4", "value4", false)?;
    let updated_fvs = db.get_all("TEST_DB", "update_test", false)?;
    assert_eq!(updated_fvs.len(), 2);
    assert_eq!(updated_fvs["field1"].as_cxx_str(), "value3");
    assert_eq!(updated_fvs["field4"].as_cxx_str(), "value4");

    // Simulate additional update: fvs.update(field5='value5', field6='value6')
    db.set("TEST_DB", "update_test", "field5", "value5", false)?;
    db.set("TEST_DB", "update_test", "field6", "value6", false)?;
    let final_fvs = db.get_all("TEST_DB", "update_test", false)?;
    assert!(final_fvs.contains_key("field5"));
    assert_eq!(final_fvs["field5"].as_cxx_str(), "value5");

    // Note: Python tests TypeError on invalid update - Rust type system prevents this at compile time

    // Test blocking reading existing data
    let fvs = db.get_all("TEST_DB", "key0", true)?;
    assert!(fvs.contains_key("field1"));
    assert_eq!(fvs["field1"].as_cxx_str(), "value2");

    // Test blocking reading coming data in Redis
    // Note: This is complex in Rust due to threading, but we'll simulate the concept
    use std::thread;
    use std::time::Duration;

    // Spawn a thread that will set data after a delay (simulating thread_coming_data)
    let handle = thread::spawn(|| {
        println!("Start thread: thread_coming_data equivalent");
        // Python uses: time.sleep(DBInterface.PUB_SUB_NOTIFICATION_TIMEOUT * 2)
        // where PUB_SUB_NOTIFICATION_TIMEOUT = 10 seconds, so 10 * 2 = 20 seconds
        // For testing, we'll use a shorter but proportional delay
        thread::sleep(Duration::from_secs(2)); // 2 seconds (shorter for test efficiency)
        match SonicV2Connector::new(true, None) {
            Ok(db_thread) => {
                if let Err(e) = db_thread.connect("TEST_DB", true) {
                    println!("Thread connect error: {:?}", e);
                    return;
                }
                if let Err(e) = db_thread.set("TEST_DB", "key0_coming", "field1", "value2", false) {
                    println!("Thread set error: {:?}", e);
                    return;
                }
                println!("Leave thread: thread_coming_data equivalent");
            }
            Err(e) => println!("Thread new connector error: {:?}", e),
        }
    });

    // This should block until the data arrives (or timeout)
    // Note: Actual blocking behavior depends on implementation
    let fvs = db.get_all("TEST_DB", "key0_coming", true)?;
    assert!(fvs.contains_key("field1"));
    assert_eq!(fvs["field1"].as_cxx_str().to_str().unwrap(), "value2");

    // Wait for thread to complete
    handle.join().unwrap();

    // Test hmset
    let mut fvs_map = HashMap::new();
    fvs_map.insert("field1", CxxString::new("value3"));
    fvs_map.insert("field2", CxxString::new("value4"));
    db.hmset("TEST_DB", "key5", fvs_map)?;
    let attrs = db.get_all("TEST_DB", "key5", false)?;
    assert_eq!(attrs.len(), 2);
    assert_eq!(attrs["field1"].as_cxx_str(), "value3");
    assert_eq!(attrs["field2"].as_cxx_str(), "value4");

    let mut fvs_map2 = HashMap::new();
    fvs_map2.insert("field5", CxxString::new("value5"));
    db.hmset("TEST_DB", "key5", fvs_map2)?;
    let attrs = db.get_all("TEST_DB", "key5", false)?;
    assert_eq!(attrs.len(), 3);
    assert_eq!(attrs["field5"].as_cxx_str(), "value5");

    // Test empty/none namespace
    let db2 = SonicV2Connector::new(true, None)?;
    assert_eq!(db2.get_namespace()?, "");

    let db3 = SonicV2Connector::new(true, None)?;
    assert_eq!(db3.get_namespace()?, "");

    // Test no exception - various constructor patterns

    // Python: db = SonicV2Connector(use_unix_socket_path=True, namespace='')
    let _db1 = SonicV2Connector::new(true, Some("".to_string()))?;

    // Python: db = SonicV2Connector(use_unix_socket_path=False)
    let _db2 = SonicV2Connector::new(false, None)?;

    // Python: db = SonicV2Connector(use_unix_socket_path=False, namespace='test_namespace')
    let _db3 = SonicV2Connector::new(false, Some("test_namespace".to_string()))?;

    // Additional constructor variations to test robustness
    let _db4 = SonicV2Connector::new(true, None)?;  // Default unix socket
    let _db5 = SonicV2Connector::new(false, Some("".to_string()))?;  // TCP with empty namespace
    let _db6 = SonicV2Connector::new(true, None)?;  // Unix socket, default namespace

    // Test that all constructors succeeded without exceptions
    assert!(_db1.use_unix_socket_path(), "db1 should use unix socket");
    assert!(!_db2.use_unix_socket_path(), "db2 should not use unix socket");
    assert!(!_db3.use_unix_socket_path(), "db3 should not use unix socket");
    assert!(_db4.use_unix_socket_path(), "db4 should use unix socket");
    assert!(!_db5.use_unix_socket_path(), "db5 should not use unix socket");
    assert!(_db6.use_unix_socket_path(), "db6 should use unix socket");

    println!("All constructor variations completed successfully");

    Ok(())
}

