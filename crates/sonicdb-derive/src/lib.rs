use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_macro_input, DeriveInput, LitStr};

/// Derive macro for sonic-db serialization/deserialization
#[proc_macro_derive(SonicDb, attributes(sonicdb))]
pub fn serde_sonicdb_derive(input: TokenStream) -> TokenStream {
    // Parse the input token stream
    let input = parse_macro_input!(input as DeriveInput);

    // Get the struct name
    let struct_name = &input.ident;

    // Process struct-level drive attributes
    let mut table_name: String = "".to_string();
    let mut key_separator: char = 'a';
    let mut db_name: String = "".to_string();
    let mut protobuf_encoded: bool = false;
    let mut is_dpu: bool = false;
    for attr in &input.attrs {
        if attr.path().is_ident("sonicdb") {
            attr.parse_nested_meta(|meta| {
                if meta.path.is_ident("table_name") {
                    let value = meta.value()?; // this parses the `=`
                    let s: LitStr = value.parse()?;
                    table_name = s.value();
                    Ok(())
                } else if meta.path.is_ident("key_separator") {
                    let value = meta.value()?; // this parses the `=`
                    let s: LitStr = value.parse()?;
                    let value_str = s.value();
                    let mut chars = value_str.chars();
                    if let (Some(c), None) = (chars.next(), chars.next()) {
                        key_separator = c;
                    } else {
                        return Err(meta.error("key_separator must be a single character"));
                    }
                    Ok(())
                } else if meta.path.is_ident("db_name") {
                    let value = meta.value()?; // this parses the `=`
                    let s: LitStr = value.parse()?;
                    db_name = s.value();
                    Ok(())
                } else if meta.path.is_ident("is_proto") {
                    let value = meta.value()?; // this parses the `=`
                    let proto_bool: LitStr = value.parse()?;
                    match proto_bool.value().to_lowercase().as_str() {
                        "true" => protobuf_encoded = true,
                        _ => protobuf_encoded = false,
                    };
                    Ok(())
                } else if meta.path.is_ident("is_dpu") {
                    let value = meta.value()?;
                    let s: LitStr = value.parse()?;
                    let value_str = s.value();
                    is_dpu = match value_str.as_str() {
                        "true" | "True" | "1" => true,
                        "false" | "False" | "0" => false,
                        _ => return Err(meta.error("is_dpu must be a boolean (true/false)")),
                    };
                    Ok(())
                } else {
                    Err(meta.error("unknown attribute"))
                }
            })
            .unwrap();
        }
    }
    if table_name.is_empty() {
        panic!("Missing table_name attribute");
    }
    if db_name.is_empty() {
        panic!("Missing db_name attribute");
    }
    if key_separator == 'a' {
        panic!("Missing key_separator attribute");
    }

    let convert_pb_to_json_impl = if protobuf_encoded {
        quote! {
            fn is_proto() -> bool {
                true
            }
            fn convert_pb_to_json(kfv: &mut swss_common::KeyOpFieldValues) {
                let value_hex = match kfv.field_values.get("pb") {
                    Some(v) => v.to_str().ok(),
                    None => None,
                };
                let value_hex = match value_hex {
                    Some(s) if !s.is_empty() => s,
                    _ => return,
                };
                let value_bytes = match hex::decode(value_hex) {
                    Ok(bytes) => bytes,
                    Err(_) => return,
                };
                let config = match #struct_name::decode(&*value_bytes) {
                    Ok(cfg) => cfg,
                    Err(_) => return,
                };

                let json = match serde_json::to_string(&config) {
                    Ok(j) => j,
                    Err(_) => return,
                };
                kfv.field_values.clear();
                kfv.field_values.insert("json".to_string(), json.into());
            }
        }
    } else {
        quote! {
            fn is_proto() -> bool {
                false
            }
        }
    };

    let is_dpu_value = is_dpu;

    let expanded = quote! {
        impl sonic_common::SonicDbTable for #struct_name {
            fn key_separator() -> char {
                #key_separator
            }

            fn table_name() -> &'static str {
                #table_name
            }

            fn db_name() -> &'static str {
                #db_name
            }

            fn is_dpu() -> bool {
                #is_dpu_value
            }

            #convert_pb_to_json_impl
        }
    };

    TokenStream::from(expanded)
}
