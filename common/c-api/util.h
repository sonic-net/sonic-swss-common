#ifndef SWSS_COMMON_C_API_UTIL_H
#define SWSS_COMMON_C_API_UTIL_H

// External utilities (c-facing)
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    char *field, *value;
} SWSSFieldValuePair;

typedef struct {
    uint64_t len;
    SWSSFieldValuePair *data;
} SWSSFieldValueArray;

typedef struct {
    char *key;
    char *operation;
    SWSSFieldValueArray fieldValues;
} SWSSKeyOpFieldValues;

typedef struct {
    uint64_t len;
    SWSSKeyOpFieldValues *data;
} SWSSKeyOpFieldValuesArray;

#ifdef __cplusplus
}
#endif

// Internal utilities (used to help define c-facing functions)
#ifdef __cplusplus
#include <cstdlib>
#include <exception>
#include <iostream>
#include <utility>

#include "../logger.h"
#include "../rediscommand.h"

// In the catch block, we must abort because passing an exception across an ffi boundary is
// undefined behavior. It was also decided that no exceptions in swss-common are recoverable, so
// there is no reason to convert exceptions into a returnable type.
#define SWSSTry(...)                                                                               \
    try {                                                                                          \
        { __VA_ARGS__; }                                                                           \
    } catch (std::exception & e) {                                                                 \
        std::cerr << "Aborting due to exception: " << e.what() << std::endl;                       \
        SWSS_LOG_ERROR("Aborting due to exception: %s", e.what());                                 \
        std::abort();                                                                              \
    }

// T is anything that has a .size() method and which can be iterated over for pair<string, string>
// eg unordered_map<string, string> or vector<pair<string, string>>
template <class T> static inline SWSSFieldValueArray makeFieldValueArray(const T &in) {
    SWSSFieldValueArray out;
    out.len = (uint64_t)in.size();
    out.data = (SWSSFieldValuePair *)malloc(out.len * sizeof(SWSSFieldValuePair));

    size_t i = 0;
    for (const auto &pair : in) {
        SWSSFieldValuePair entry;
        entry.field = strdup(pair.first.c_str());
        entry.value = strdup(pair.second.c_str());
        out.data[i++] = entry;
    }

    return out;
}

static inline std::vector<swss::FieldValueTuple> takeFieldValueArray(SWSSFieldValueArray in) {
    std::vector<swss::FieldValueTuple> out;
    for (uint64_t i = 0; i < in.len; i++) {
        auto field = std::string(in.data[i].field);
        auto value = std::string(in.data[i].value);
        out.push_back(std::make_pair(field, value));
    }
    return out;
}

static inline SWSSKeyOpFieldValues makeKeyOpFieldValues(const swss::KeyOpFieldsValuesTuple &kfv) {
    SWSSKeyOpFieldValues out;
    out.key = strdup(kfvKey(kfv).c_str());
    out.operation = strdup(kfvOp(kfv).c_str());
    out.fieldValues = makeFieldValueArray(kfvFieldsValues(kfv));
    return out;
}

// T is anything that has a .size() method and which can be iterated over for KeyOpFieldValuesTuple
template <class T> static inline SWSSKeyOpFieldValuesArray makeKeyOpFieldValuesArray(const T &in) {
    SWSSKeyOpFieldValuesArray out;
    out.len = (uint64_t)in.size();
    out.data = (SWSSKeyOpFieldValues *)malloc(out.len * sizeof(SWSSKeyOpFieldValues));

    size_t i = 0;
    for (const auto &kfv : in)
        out.data[i++] = makeKeyOpFieldValues(kfv);

    return out;
}

#endif
#endif
