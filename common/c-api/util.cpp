#include "util.h"

using namespace swss;

SWSSString SWSSString_new(const char *data, uint64_t length) {
    return makeString(std::string(data, numeric_cast<std::string::size_type>(length)));
}

SWSSString SWSSString_new_c_str(const char *c_str) {
    return makeString(std::string(c_str));
}

const char *SWSSStrRef_c_str(SWSSStrRef s) {
    return ((std::string *)s)->c_str();
}

uint64_t SWSSStrRef_length(SWSSStrRef s) {
    return ((std::string *)s)->length();
}

void SWSSString_free(SWSSString s) {
    delete (std::string *)s;
}

void SWSSFieldValueArray_free(SWSSFieldValueArray arr) {
    delete[] arr.data;
}

void SWSSKeyOpFieldValuesArray_free(SWSSKeyOpFieldValuesArray kfvs) {
    delete[] kfvs.data;
}

void SWSSStringArray_free(SWSSStringArray arr) {
    delete[] arr.data;
}
