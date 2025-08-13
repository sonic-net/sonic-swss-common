#include <cstring>
#include <string>
#include <map>

#include "../events.h"
#include "events.h"
#include "util.h"

using namespace std;

struct SWSSEventPublisherOpaque {
    event_handle_t handle;

    SWSSEventPublisherOpaque(event_handle_t h) : handle(h) {}
};

SWSSResult SWSSEventPublisher_new(const char *event_source, SWSSEventPublisher *outPublisher) {
    SWSSTry({
        string source_str = event_source ? string(event_source) : "";
        event_handle_t handle = events_init_publisher(source_str);
        if (handle == nullptr) {
            throw std::runtime_error("Failed to initialize event publisher");
        }
        *outPublisher = new SWSSEventPublisherOpaque(handle);
    });
}

SWSSResult SWSSEventPublisher_deinit(SWSSEventPublisher publisher) {
    SWSSTry({
        if (publisher) {
            events_deinit_publisher(publisher->handle);
            publisher->handle = nullptr;
        }
    });
}

SWSSResult SWSSEventPublisher_free(SWSSEventPublisher publisher) {
    SWSSTry({
        if (publisher) {
            if (publisher->handle) {
                events_deinit_publisher(publisher->handle);
            }
            delete publisher;
        }
    });
}

SWSSResult SWSSEventPublisher_publish(SWSSEventPublisher publisher, const char *event_tag, const SWSSFieldValueArray *params) {
    SWSSTry({
        if (!publisher || !event_tag) {
            throw std::invalid_argument("Invalid publisher or event_tag");
        }

        string tag_str(event_tag);
        event_params_t event_params;

        // Convert SWSSFieldValueArray to event_params_t if params provided
        if (params) {
            for (uint64_t i = 0; i < params->len; i++) {
                string key(params->data[i].field);
                string value = takeString(std::move(params->data[i].value));
                event_params[key] = value;
            }
        }

        int result = event_publish(publisher->handle, tag_str, params ? &event_params : nullptr);
        if (result != 0) {
            throw std::runtime_error("Failed to publish event, error code: " + std::to_string(result));
        }
    });
}