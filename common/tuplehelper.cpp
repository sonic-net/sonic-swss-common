#include "tuplehelper.h"

using namespace std;

namespace swss {

std::string TupleHelper::getField(const FieldValueTuple& tuple)
{
    return fvField(tuple);
}

std::string TupleHelper::getValue(const FieldValueTuple& tuple)
{
    return fvValue(tuple);
}

std::string TupleHelper::getKey(const KeyOpFieldsValuesTuple& tuple)
{
    return kfvKey(tuple);
}

std::string TupleHelper::getOp(const KeyOpFieldsValuesTuple& tuple)
{
    return kfvOp(tuple);
}

std::vector<FieldValueTuple> TupleHelper::getFieldsValues(const KeyOpFieldsValuesTuple& tuple)
{
    return kfvFieldsValues(tuple);
}

}
