#ifndef SWSS_COMMON_C_API_EVENTS_H
#define SWSS_COMMON_C_API_EVENTS_H

#include "result.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSEventPublisherOpaque *SWSSEventPublisher;

// Initialize an event publisher for a given source
// event_source: The YANG module name for the event source
SWSSResult SWSSEventPublisher_new(const char *event_source, SWSSEventPublisher *outPublisher);

// Deinitialize the event publisher (without freeing the handle)
SWSSResult SWSSEventPublisher_deinit(SWSSEventPublisher publisher);

// Free the event publisher
SWSSResult SWSSEventPublisher_free(SWSSEventPublisher publisher);

// Publish an event with the given tag and parameters
// publisher: Event publisher handle
// event_tag: Name of the YANG container that defines this event
// params: Parameters associated with the event (can be NULL)
SWSSResult SWSSEventPublisher_publish(SWSSEventPublisher publisher, const char *event_tag, const SWSSFieldValueArray *params);

#ifdef __cplusplus
}
#endif

#endif