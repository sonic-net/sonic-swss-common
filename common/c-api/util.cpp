#include "util.h"

using namespace swss;

bool swss::cApiTestingDisableAbort = false;

SWSSString SWSSString_new(const char *data, uint64_t length) {
    SWSSTry(return makeString(std::string(data, length)));
}

SWSSString SWSSString_new_c_str(const char *c_str) {
    SWSSTry(return makeString(std::string(c_str)));
}

const char *SWSSStrRef_c_str(SWSSStrRef s) {
    SWSSTry(return ((std::string *)s)->c_str());
}

uint64_t SWSSStrRef_length(SWSSStrRef s) {
    SWSSTry(return ((std::string *)s)->length());
}

void SWSSString_free(SWSSString s) {
    SWSSTry(delete (std::string *)s);
}

void SWSSFieldValueArray_free(SWSSFieldValueArray arr) {
    SWSSTry(delete[] arr.data);
}

void SWSSKeyOpFieldValuesArray_free(SWSSKeyOpFieldValuesArray kfvs) {
    SWSSTry(delete[] kfvs.data);
}
