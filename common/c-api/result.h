#ifndef SWSS_COMMON_C_API_RESULT_H
#define SWSS_COMMON_C_API_RESULT_H

#include "util.h"

typedef enum {
    // No exception occurred
    SWSSException_None,

    // std::exception was thrown
    SWSSException_Exception,
} SWSSException;

// If exception is SWSSException_None, message and location will be null.
// If message and location are non-null, they must be freed using SWSSString_free.
typedef struct {
    SWSSException exception;
    SWSSString message;
    SWSSString location;
} SWSSResult;

#ifdef __cplusplus
#include <exception>
#include <string>
#include <boost/current_function.hpp>

using namespace swss;

#define SWSSTry(...) return SWSSTry_([=] { __VA_ARGS__; }, BOOST_CURRENT_FUNCTION)

template <class Func> static inline SWSSResult SWSSTry_(Func &&f, const char *funcName) {
    SWSSResult result = {SWSSException_None, nullptr, nullptr};
    try {
        f();
    } catch (std::exception &e) {
        result.exception = SWSSException_Exception;
        result.message = makeString(e.what());
    }
    if (result.exception != SWSSException_None)
        result.location = makeString(funcName);
    return result;
}
#endif

#endif
