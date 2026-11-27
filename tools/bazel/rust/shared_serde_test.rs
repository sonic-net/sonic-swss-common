use swss_common::CxxString;

#[test]
fn common_string_implements_shared_serde_traits() {
    fn check_traits<T: serde::Serialize + serde::de::DeserializeOwned>() {}
    check_traits::<CxxString>();

    let input = serde::de::value::StrDeserializer::<serde::de::value::Error>::new("shared serde");
    let value: CxxString = serde::Deserialize::deserialize(input).unwrap();
    assert_eq!(value.to_str().unwrap(), "shared serde");
}

#[test]
fn common_string_implements_shared_serde_core_traits() {
    fn check_traits<T: serde_core::Serialize + serde_core::de::DeserializeOwned>() {}
    check_traits::<CxxString>();

    let input =
        serde_core::de::value::StrDeserializer::<serde_core::de::value::Error>::new("shared core");
    let value: CxxString = serde_core::Deserialize::deserialize(input).unwrap();
    assert_eq!(value.to_str().unwrap(), "shared core");
}
