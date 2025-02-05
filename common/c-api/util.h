#ifndef SWSS_COMMON_C_API_UTIL_H
#define SWSS_COMMON_C_API_UTIL_H

// External utilities (c-facing)
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// FFI version of std::string&&
// This can be converted to an SWSSStrRef with a standard cast
typedef struct SWSSStringOpaque *SWSSString;

// FFI version of std::string&
// This can be converted to an SWSSString with a standard cast
// Functions that take SWSSString will move data out of the underlying string,
// but functions that take SWSSStrRef will only view it.
typedef struct SWSSStrRefOpaque *SWSSStrRef;

// FFI version of swss::FieldValueTuple
// field should be freed with libc's free()
// value should be freed with SWSSString_free()
typedef struct {
    const char *field;
    SWSSString value;
} SWSSFieldValueTuple;

// FFI version of std::vector<swss::FieldValueTuple>
// data should be freed with SWSSFieldValueArray_free()
typedef struct {
    uint64_t len;
    SWSSFieldValueTuple *data;
} SWSSFieldValueArray;

typedef enum {
    SWSSKeyOperation_SET,
    SWSSKeyOperation_DEL,
} SWSSKeyOperation;

// FFI version of swss::KeyOpFieldValuesTuple
// key should be freed with libc's free()
// fieldValues should be freed with SWSSFieldValueArray_free()
typedef struct {
    const char *key;
    SWSSKeyOperation operation;
    SWSSFieldValueArray fieldValues;
} SWSSKeyOpFieldValues;

// FFI version of std::vector<swss::KeyOpFieldValueTuple>
// data should be freed with SWSSKeyOpFieldValuesArray_free()
typedef struct {
    uint64_t len;
    SWSSKeyOpFieldValues *data;
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

// FFI version of std::vector<std::string>
// strings in data should be freed with libc's free()
// data should be freed with SWSSStringArray_free()
typedef struct {
    uint64_t len;
    const char **data;
} SWSSStringArray;

// data should not include a null terminator
SWSSString SWSSString_new(const char *data, uint64_t length);

// c_str should include a null terminator
SWSSString SWSSString_new_c_str(const char *c_str);

// It is safe to pass null to this function (not to any other SWSSString functions). This is
// useful to take SWSSStrings from other SWSS structs - you can replace the strs in the
// structs with null and still safely free the structs. Then, you can call this function with the
// populated SWSSString later.
void SWSSString_free(SWSSString s);

const char *SWSSStrRef_c_str(SWSSStrRef s);

// Returns the length of the string, not including the null terminator that is implicitly added by
// SWSSStrRef_c_str.
uint64_t SWSSStrRef_length(SWSSStrRef s);

// arr.data may be null. This is not recursive - elements must be freed separately (for finer
// grained control of ownership).
void SWSSFieldValueArray_free(SWSSFieldValueArray arr);

// kfvs.data may be null. This is not recursive - elements must be freed separately (for finer
// grained control of ownership).
void SWSSKeyOpFieldValuesArray_free(SWSSKeyOpFieldValuesArray kfvs);

// arr.data may be null. This is not recursive - elements must be freed separately (for finer
// grained control of ownership).
void SWSSStringArray_free(SWSSStringArray arr);

#ifdef __cplusplus
}
#endif

// Internal utilities (used to help define c-facing functions)
#ifdef __cplusplus

#include <boost/cast.hpp>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../logger.h"
#include "../redisapi.h"
#include "../schema.h"
#include "../select.h"

using boost::numeric_cast;

static inline SWSSSelectResult selectOne(swss::Selectable *s, uint32_t timeout_ms,
                                         uint8_t interrupt_on_signal) {
    swss::Select select;
    swss::Selectable *sOut;
    select.addSelectable(s);
    int ret = select.select(&sOut, numeric_cast<int>(timeout_ms), interrupt_on_signal);
    switch (ret) {
    case swss::Select::OBJECT:
        return SWSSSelectResult_DATA;
    case swss::Select::ERROR:
        throw std::system_error(errno, std::generic_category());
    case swss::Select::TIMEOUT:
        return SWSSSelectResult_TIMEOUT;
    case swss::Select::SIGNALINT:
        return SWSSSelectResult_SIGNAL;
    default:
        SWSS_LOG_THROW("impossible: unhandled Select::select() return value: %d", ret);
    }
}

static inline SWSSString makeString(std::string &&s) {
    std::string *data_s = new std::string(std::move(s));
    return (struct SWSSStringOpaque *)data_s;
}

// T is anything that has a .size() method and which can be iterated over for pair<string, string>
// eg vector<pair<string, string>>
template <class T> static inline SWSSFieldValueArray makeFieldValueArray(T &&in) {
    SWSSFieldValueTuple *data = new SWSSFieldValueTuple[in.size()];

    size_t i = 0;
    for (auto &pair : in) {
        SWSSFieldValueTuple entry;
        entry.field = strdup(pair.first.c_str());
        entry.value = makeString(std::move(pair.second));
        data[i++] = entry;
    }

    SWSSFieldValueArray out;
    out.len = (uint64_t)in.size();
    out.data = data;
    return out;
}

static inline SWSSKeyOperation makeKeyOperation(std::string &op) {
    if (strcmp(op.c_str(), SET_COMMAND) == 0) {
        return SWSSKeyOperation_SET;
    } else if (strcmp(op.c_str(), DEL_COMMAND) == 0) {
        return SWSSKeyOperation_DEL;
    } else {
        SWSS_LOG_THROW("Invalid key operation %s", op.c_str());
    }
}

static inline SWSSKeyOpFieldValues makeKeyOpFieldValues(swss::KeyOpFieldsValuesTuple &&in) {
    SWSSKeyOpFieldValues out;
    out.key = strdup(kfvKey(in).c_str());
    out.operation = makeKeyOperation(kfvOp(in));
    out.fieldValues = makeFieldValueArray(kfvFieldsValues(in));
    return out;
}

template <class T> static inline T &getReference(T &t) {
    return t;
}

template <class T> static inline T &getReference(std::shared_ptr<T> &t) {
    return *t;
}

// T is anything that has a .size() method and which can be iterated over for
// swss::KeyOpFieldValuesTuple, eg vector or deque
template <class T> static inline SWSSKeyOpFieldValuesArray makeKeyOpFieldValuesArray(T &&in) {
    SWSSKeyOpFieldValues *data = new SWSSKeyOpFieldValues[in.size()];

    size_t i = 0;
    for (auto &kfv : in)
        data[i++] = makeKeyOpFieldValues(std::move(getReference(kfv)));

    SWSSKeyOpFieldValuesArray out;
    out.len = (uint64_t)in.size();
    out.data = data;
    return out;
}

static inline SWSSStringArray makeStringArray(std::vector<std::string> &&in) {
    const char **data = new const char*[in.size()];

    size_t i = 0;
    for (std::string &s : in)
        data[i++] = strdup(s.c_str());

    SWSSStringArray out;
    out.len = (uint64_t)in.size();
    out.data = data;
    return out;
}

static inline std::string takeString(SWSSString s) {
    return std::string(std::move(*((std::string *)s)));
}

static inline std::string &takeStrRef(SWSSStrRef s) {
    return *((std::string *)s);
}

static inline std::vector<swss::FieldValueTuple> takeFieldValueArray(SWSSFieldValueArray in) {
    std::vector<swss::FieldValueTuple> out;
    for (uint64_t i = 0; i < in.len; i++) {
        const char *field = in.data[i].field;
        SWSSString value = in.data[i].value;
        auto pair = std::make_pair(std::string(field), takeString(std::move(value)));
        out.push_back(pair);
    }
    return out;
}

static inline std::string takeKeyOperation(SWSSKeyOperation op) {
    switch (op) {
    case SWSSKeyOperation_SET:
        return SET_COMMAND;
    case SWSSKeyOperation_DEL:
        return DEL_COMMAND;
    default:
        SWSS_LOG_THROW("Impossible SWSSKeyOperation");
    }
}

static inline swss::KeyOpFieldsValuesTuple takeKeyOpFieldValues(SWSSKeyOpFieldValues in) {
    std::string key = in.key;
    std::string op = takeKeyOperation(in.operation);
    auto fieldValues = takeFieldValueArray(in.fieldValues);
    return std::make_tuple(key, op, fieldValues);
}

static inline std::vector<swss::KeyOpFieldsValuesTuple>
takeKeyOpFieldValuesArray(SWSSKeyOpFieldValuesArray in) {
    std::vector<swss::KeyOpFieldsValuesTuple> out;
    for (uint64_t i = 0; i < in.len; i++) {
        SWSSKeyOpFieldValues kfv = in.data[i];
        out.push_back(takeKeyOpFieldValues(std::move(kfv)));
    }
    return out;
}

#endif
#endif
