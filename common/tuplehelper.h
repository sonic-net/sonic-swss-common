#ifndef __TUPLE_HELPER__
#define __TUPLE_HELPER__

#include <string>
#include <vector>

#include "rediscommand.h"

namespace swss {

/*
    SWIG not support std::tuple https://www.swig.org/Doc4.0/SWIGDocumentation.html#CPlusPlus11_tuple_types
 */
class TupleHelper {
public:
    static const std::string& getField(const FieldValueTuple& tuple);
    
    static const std::string& getValue(const FieldValueTuple& tuple);

    static const std::string& getKey(const KeyOpFieldsValuesTuple& tuple);
    
    static const std::string& getOp(const KeyOpFieldsValuesTuple& tuple);
    
    static const std::vector<FieldValueTuple>& getFieldsValues(const KeyOpFieldsValuesTuple& tuple);
};

}
#endif
