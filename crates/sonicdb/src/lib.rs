use swss_common::KeyOpFieldValues;

/// Trait for objects that can be stored in a Sonic DB table.
pub trait SonicDbTable {
    fn key_separator() -> char;
    fn table_name() -> &'static str;
    fn db_name() -> &'static str;
    fn is_proto() -> bool {
        false
    }
    fn convert_pb_to_json(_kfv: &mut KeyOpFieldValues) {
        // Default implementation does nothing.
        // This can be overridden by the macro to convert protobuf to JSON.
    }
    fn is_dpu() -> bool;
}
