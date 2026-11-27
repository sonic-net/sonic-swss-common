#include "swss/c-api/util.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    static const char expected[] = "sonic-swss-common";
    SWSSString value = SWSSString_new_c_str(expected);
    if (value == NULL) {
        fprintf(stderr, "SWSSString_new_c_str returned NULL\n");
        return 1;
    }

    SWSSStrRef ref = (SWSSStrRef)value;
    const char *actual = SWSSStrRef_c_str(ref);
    int matches = actual != NULL &&
                  SWSSStrRef_length(ref) == sizeof(expected) - 1 &&
                  strcmp(actual, expected) == 0;
    SWSSString_free(value);

    if (!matches) {
        fprintf(stderr, "C API string roundtrip failed\n");
        return 1;
    }
    return 0;
}
