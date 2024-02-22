#ifndef __ARM_HELPER__
#define __ARM_HELPER__

#if defined(__arm__) || defined(__aarch64__)
#define WARNINGS_NO_CAST_ALIGN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wcast-align\"")
#define WARNINGS_RESET \
    _Pragma ("GCC diagnostic pop")
#else
#define WARNINGS_NO_CAST_ALIGN
#define WARNINGS_RESET
#endif

#endif
