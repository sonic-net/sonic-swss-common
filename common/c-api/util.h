#ifndef SWSS_COMMON_C_API_UTIL_H
#define SWSS_COMMON_C_API_UTIL_H

// External utilities (c-facing)
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    const char *field;
    const char *value;
} SWSSFieldValuePair;

typedef struct {
    uint64_t len;
    const SWSSFieldValuePair *data;
} SWSSFieldValueArray;

typedef struct {
    const char *key;
    const char *operation;
    SWSSFieldValueArray fieldValues;
} SWSSKeyOpFieldValues;

typedef struct {
    uint64_t len;
    const SWSSKeyOpFieldValues *data;
} SWSSKeyOpFieldValuesArray;

// FFI version of swss::Select::{OBJECT, TIMEOUT, SIGNALINT}.
// swss::Select::ERROR is left out because errors are handled separately
typedef enum {
    // Data is available in the object
    SWSSSelectResult_DATA = 0,
    // Timed out waiting for data
    SWSSSelectResult_TIMEOUT = 1,
    // Waiting was interrupted by a signal
    SWSSSelectResult_SIGNAL = 2,
} SWSSSelectResult;

#ifdef __cplusplus
}
#endif

// Internal utilities (used to help define c-facing functions)
#ifdef __cplusplus
#include <boost/numeric/conversion/cast.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <tuple>
#include <utility>

#include "../logger.h"
#include "../rediscommand.h"
#include "../select.h"

using boost::numeric_cast;

namespace swss {

extern bool cApiTestingDisableAbort;

// In the catch block, we must abort because passing an exception across an ffi boundary is
// undefined behavior. It was also decided that no exceptions in swss-common are recoverable, so
// there is no reason to convert exceptions into a returnable type.
#define SWSSTry(...)                                                                               \
    if (cApiTestingDisableAbort) {                                                                 \
        { __VA_ARGS__; }                                                                           \
    } else {                                                                                       \
        try {                                                                                      \
            { __VA_ARGS__; }                                                                       \
        } catch (std::exception & e) {                                                             \
            std::cerr << "Aborting due to exception: " << e.what() << std::endl;                   \
            SWSS_LOG_ERROR("Aborting due to exception: %s", e.what());                             \
            std::abort();                                                                          \
        }                                                                                          \
    }

static inline SWSSSelectResult selectOne(swss::Selectable *s, uint32_t timeout_ms,
                                         uint8_t interrupt_on_signal) {
    Select select;
    Selectable *sOut;
    select.addSelectable(s);
    int ret = select.select(&sOut, numeric_cast<int>(timeout_ms), interrupt_on_signal);
    switch (ret) {
    case Select::OBJECT:
        return SWSSSelectResult_DATA;
    case Select::ERROR:
        throw std::system_error(errno, std::generic_category());
    case Select::TIMEOUT:
        return SWSSSelectResult_TIMEOUT;
    case Select::SIGNALINT:
        return SWSSSelectResult_SIGNAL;
    default:
        SWSS_LOG_THROW("impossible: unhandled Select::select() return value: %d", ret);
    }
}

// malloc() with safe numeric casting of the size parameter
template <class N> static inline void *mallocN(N size) {
    return malloc(numeric_cast<size_t>(size));
}

// T is anything that has a .size() method and which can be iterated over for pair<string, string>
// eg unordered_map<string, string> or vector<pair<string, string>>
template <class T> static inline SWSSFieldValueArray makeFieldValueArray(const T &in) {
    SWSSFieldValuePair *data =
        (SWSSFieldValuePair *)mallocN(in.size() * sizeof(SWSSFieldValuePair));

    size_t i = 0;
    for (const auto &pair : in) {
        SWSSFieldValuePair entry;
        entry.field = strdup(pair.first.c_str());
        entry.value = strdup(pair.second.c_str());
        data[i++] = entry;
    }

    SWSSFieldValueArray out;
    out.len = (uint64_t)in.size();
    out.data = data;
    return out;
}

static inline std::vector<swss::FieldValueTuple>
takeFieldValueArray(const SWSSFieldValueArray &in) {
    std::vector<swss::FieldValueTuple> out;
    for (uint64_t i = 0; i < in.len; i++) {
        auto field = std::string(in.data[i].field);
        auto value = std::string(in.data[i].value);
        out.push_back(std::make_pair(field, value));
    }
    return out;
}

static inline SWSSKeyOpFieldValues makeKeyOpFieldValues(const swss::KeyOpFieldsValuesTuple &in) {
    SWSSKeyOpFieldValues out;
    out.key = strdup(kfvKey(in).c_str());
    out.operation = strdup(kfvOp(in).c_str());
    out.fieldValues = makeFieldValueArray(kfvFieldsValues(in));
    return out;
}

static inline swss::KeyOpFieldsValuesTuple takeKeyOpFieldValues(const SWSSKeyOpFieldValues &in) {
    std::string key(in.key), op(in.operation);
    auto fieldValues = takeFieldValueArray(in.fieldValues);
    return std::make_tuple(key, op, fieldValues);
}

template <class T> static inline const T &getReference(const T &t) {
    return t;
}

template <class T> static inline const T &getReference(const std::shared_ptr<T> &t) {
    return *t;
}

// T is anything that has a .size() method and which can be iterated over for
// swss::KeyOpFieldValuesTuple
template <class T> static inline SWSSKeyOpFieldValuesArray makeKeyOpFieldValuesArray(const T &in) {
    SWSSKeyOpFieldValues *data =
        (SWSSKeyOpFieldValues *)mallocN(in.size() * sizeof(SWSSKeyOpFieldValues));

    size_t i = 0;
    for (const auto &kfv : in)
        data[i++] = makeKeyOpFieldValues(getReference(kfv));

    SWSSKeyOpFieldValuesArray out;
    out.len = (uint64_t)in.size();
    out.data = data;
    return out;
}

static inline std::vector<swss::KeyOpFieldsValuesTuple>
takeKeyOpFieldValuesArray(const SWSSKeyOpFieldValuesArray &in) {
    std::vector<swss::KeyOpFieldsValuesTuple> out;
    for (uint64_t i = 0; i < in.len; i++)
        out.push_back(takeKeyOpFieldValues(in.data[i]));
    return out;
}

} // namespace swss

#endif
#endif
