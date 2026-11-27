fn main() {
    // A foreign type must implement the Serde traits from the caller's graph.
    let encoded = serde_json::to_string(&swss_common::KeyOperation::Set).unwrap();
    assert_eq!(encoded, "\"Set\"");
    let decoded: swss_common::KeyOperation = serde_json::from_str("\"Del\"").unwrap();
    assert_eq!(decoded, swss_common::KeyOperation::Del);

    // The original Rust wrapper must call the actual owner C API.
    let missing = std::path::PathBuf::from(std::env::var_os("TEST_TMPDIR").unwrap())
        .join("missing-common-config.json");
    assert!(!missing.exists());
    assert!(swss_common::sonic_db_config_initialize(missing.to_str().unwrap()).is_err());
    println!("4b Rust source, caller Serde graph and owner Common C ABI passed");
}
