#![allow(unused)]
use sonicdb::SonicDbTable;
use sonicdb_derive::SonicDb;
#[test]
fn test_attributes() {
    #[derive(SonicDb)]
    #[sonicdb(table_name = "MY_STRUCT", key_separator = ":", db_name = "db1")]
    struct MyStruct {
        id1: String,
        id2: String,

        attr1: Option<String>,
        attr2: Option<String>,
    }

    let obj = MyStruct {
        id1: "id1".to_string(),
        id2: "id2".to_string(),
        attr1: Some("attr1".to_string()),
        attr2: Some("attr2".to_string()),
    };

    assert!(MyStruct::key_separator() == ':');
    assert!(MyStruct::table_name() == "MY_STRUCT");
    assert!(MyStruct::db_name() == "db1");
}
