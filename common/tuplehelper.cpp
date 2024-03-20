#include "tuplehelper.h"

using namespace std;

namespace swss {

const std::string& TupleHelper::getField(const FieldValueTuple& tuple)
{
    return fvField(tuple);
}

const std::string& TupleHelper::getValue(const FieldValueTuple& tuple)
{
    return fvValue(tuple);
}

const std::string& TupleHelper::getKey(const KeyOpFieldsValuesTuple& tuple)
{
    return kfvKey(tuple);
}

const std::string& TupleHelper::getOp(const KeyOpFieldsValuesTuple& tuple)
{
    return kfvOp(tuple);
}

const std::vector<FieldValueTuple>& TupleHelper::getFieldsValues(const KeyOpFieldsValuesTuple& tuple)
{
    return kfvFieldsValues(tuple);
}

}
