use serial_test::serial;
use std::collections::HashMap;
use swss_common::{ConfigDBConnector, ConfigDBPipeConnector, ConfigDbRawMap, CxxString};
use swss_common_testing::Redis;

fn row(fields: &[(&str, &str)]) -> HashMap<String, CxxString> {
    fields
        .iter()
        .map(|(field, value)| ((*field).to_string(), CxxString::new(value)))
        .collect()
}

#[test]
#[serial]
fn mod_config_raw_uses_existing_native_bulk_write_semantics(
) -> Result<(), Box<dyn std::error::Error>> {
    let _redis = Redis::start_config_db();
    let reader = ConfigDBConnector::new(true, None)?;
    reader.connect(false, false)?;
    reader.get_redis_client("CONFIG_DB")?.flush_db()?;

    let pipe = ConfigDBPipeConnector::new(true, None)?;
    pipe.connect(false, false)?;
    let config: ConfigDbRawMap = HashMap::from([
        (
            "PIPE_TEST".to_string(),
            HashMap::from([
                ("row-a".to_string(), row(&[("field-a", "value-a")])),
                ("row-b".to_string(), row(&[("field-b", "value-b")])),
            ]),
        ),
        (
            "PIPE_TEST_SECOND".to_string(),
            HashMap::from([("row-c".to_string(), row(&[("field-c", "value-c")]))]),
        ),
    ]);
    pipe.mod_config_raw(&config)?;
    assert_eq!(
        reader.get_entry("PIPE_TEST", "row-b")?["field-b"].as_bytes(),
        b"value-b"
    );

    let complete_config = pipe.get_config()?;
    assert_eq!(
        complete_config["PIPE_TEST"]["row-a"]["field-a"].as_bytes(),
        b"value-a"
    );
    assert_eq!(
        complete_config["PIPE_TEST"]["row-b"]["field-b"].as_bytes(),
        b"value-b"
    );
    assert_eq!(
        complete_config["PIPE_TEST_SECOND"]["row-c"]["field-c"].as_bytes(),
        b"value-c"
    );

    // Empty row and table maps use native deletion semantics.
    pipe.mod_config_raw(&HashMap::from([(
        "PIPE_TEST".to_string(),
        HashMap::from([("row-a".to_string(), HashMap::new())]),
    )]))?;
    assert!(reader.get_entry("PIPE_TEST", "row-a")?.is_empty());
    pipe.mod_config_raw(&HashMap::from([("PIPE_TEST".to_string(), HashMap::new())]))?;
    assert!(reader.get_table("PIPE_TEST")?.is_empty());
    Ok(())
}

#[test]
fn connector_retains_connection_options() -> Result<(), Box<dyn std::error::Error>> {
    let pipe = ConfigDBPipeConnector::new(false, Some("asic0".to_string()))?;
    assert!(!pipe.use_unix_socket_path());
    assert_eq!(pipe.netns(), "asic0");
    Ok(())
}
