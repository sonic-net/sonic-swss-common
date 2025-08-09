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

void SWSSConfigMap_free(SWSSConfigMap config) {
    if (config.data) {
        for (uint64_t i = 0; i < config.len; i++) {
            SWSSConfigTable &table = config.data[i];

            // Free table name
            free(const_cast<char*>(table.table_name));

            // Free entries array and its contents
            if (table.entries.data) {
                for (uint64_t j = 0; j < table.entries.len; j++) {
                    SWSSKeyOpFieldValues &kfv = table.entries.data[j];

                    // Free key
                    free(const_cast<char*>(kfv.key));

                    // Free field-value array
                    if (kfv.fieldValues.data) {
                        for (uint64_t k = 0; k < kfv.fieldValues.len; k++) {
                            SWSSFieldValueTuple &fv = kfv.fieldValues.data[k];

                            // Free field name
                            free(const_cast<char*>(fv.field));

                            // Free value string - SWSSString_free handles NULL
                            SWSSString_free(fv.value);
                        }
                        delete[] kfv.fieldValues.data;
                    }
                }
                delete[] table.entries.data;
            }
        }
        delete[] config.data;
    }
}
