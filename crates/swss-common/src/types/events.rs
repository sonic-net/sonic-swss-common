use super::*;
use crate::bindings::*;
use std::collections::HashMap;

/// Rust wrapper around `SWSSEventPublisher`.
#[derive(Debug)]
pub struct EventPublisher {
    pub(crate) ptr: SWSSEventPublisher,
    event_source: String,
}

impl EventPublisher {
    /// Create a new EventPublisher.
    ///
    /// # Arguments
    /// * `event_source` - The YANG module name for the event source
    pub fn new(event_source: &str) -> Result<EventPublisher> {
        let source_cstr = cstr(event_source)?;

        let ptr = unsafe {
            swss_try!(p_publisher => SWSSEventPublisher_new(
                source_cstr.as_ptr(),
                p_publisher
            ))?
        };

        Ok(Self {
            ptr,
            event_source: event_source.to_string(),
        })
    }

    /// Publish an event with the given tag and parameters.
    ///
    /// # Arguments
    /// * `event_tag` - Name of the YANG container that defines this event
    /// * `params` - Parameters associated with the event (optional)
    pub fn publish(&self, event_tag: &str, params: Option<&HashMap<String, String>>) -> Result<()> {
        let tag_cstr = cstr(event_tag)?;

        unsafe {
            if let Some(params_map) = params {
                // Keep CStrings alive for the duration of the call
                let keys_cstrs: Vec<_> = params_map.keys().map(|k| cstr(k)).collect::<Result<Vec<_>>>()?;
                let values_cstrs: Vec<_> = params_map.values().map(|v| cstr(v)).collect::<Result<Vec<_>>>()?;

                // Convert HashMap to SWSSFieldValueArray
                let mut field_values: Vec<SWSSFieldValueTuple> = keys_cstrs
                    .iter()
                    .zip(values_cstrs.iter())
                    .map(|(key_cstr, value_cstr)| {
                        let value_str = SWSSString_new_c_str(value_cstr.as_ptr());
                        SWSSFieldValueTuple {
                            field: key_cstr.as_ptr(),
                            value: value_str,
                        }
                    })
                    .collect();

                let params_array = SWSSFieldValueArray {
                    data: field_values.as_mut_ptr(),
                    len: field_values.len() as u64,
                };

                let result = swss_try!(SWSSEventPublisher_publish(
                    self.ptr,
                    tag_cstr.as_ptr(),
                    &params_array
                ));

                // Clean up SWSSStrings
                for field_value in &field_values {
                    SWSSString_free(field_value.value);
                }

                result
            } else {
                swss_try!(SWSSEventPublisher_publish(
                    self.ptr,
                    tag_cstr.as_ptr(),
                    std::ptr::null()
                ))
            }
        }
    }

    /// Deinitialize the event publisher - matches Python swsscommon.events_deinit_publisher()
    pub fn deinit(&mut self) -> Result<()> {
        unsafe {
            swss_try!(SWSSEventPublisher_deinit(self.ptr))
        }
    }

    /// Get the event source associated with this publisher.
    pub fn event_source(&self) -> &str {
        &self.event_source
    }
}

impl Drop for EventPublisher {
    fn drop(&mut self) {
        unsafe {
            let _ = SWSSEventPublisher_free(self.ptr);
        }
    }
}

// Safety: EventPublisher can be safely sent between threads
unsafe impl Send for EventPublisher {}
// Safety: EventPublisher can be safely shared between threads with proper synchronization
unsafe impl Sync for EventPublisher {}