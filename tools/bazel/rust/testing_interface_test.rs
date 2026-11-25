use swss_common::{CxxString, DbConnector, FieldValues, KeyOpFieldValues};
use swss_common_testing::{random_fvs, random_kfv, Redis};

#[test]
fn testing_helpers_return_the_consumers_common_types() {
    // This fails to compile if the testing helper selected another Common
    // feature variant, even if both variants have identically named types.
    let _: fn(&Redis) -> DbConnector = Redis::db_connector;
    let fields: FieldValues = random_fvs();
    assert!(!fields.is_empty());
    assert!(fields
        .values()
        .all(|value| !value.to_str().unwrap().is_empty()));
    let _: KeyOpFieldValues = random_kfv();
}

#[test]
fn testing_variant_preserves_shared_serde_traits() {
    fn check_traits<T: serde::Serialize + serde::de::DeserializeOwned>() {}
    check_traits::<CxxString>();
    let input = serde::de::value::StrDeserializer::<serde::de::value::Error>::new("shared testing");
    let value: CxxString = serde::Deserialize::deserialize(input).unwrap();
    assert_eq!(value.to_str().unwrap(), "shared testing");
}

#[cfg(feature = "async")]
#[test]
fn async_common_runs_with_the_shared_tokio_runtime() {
    let runtime = tokio::runtime::Builder::new_current_thread()
        .build()
        .unwrap();
    // An invalid C string fails before accessing a socket. This executes
    // Common's async blocking-task adapter without a host Redis dependency.
    let result = runtime.block_on(DbConnector::new_unix_async(0, "invalid\0socket", 1));
    assert!(result.is_err());
}
