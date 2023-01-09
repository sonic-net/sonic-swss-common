#include <iostream>
#include "gtest/gtest.h"

#include "common/table.h"
#include "common/binaryserializer.h"

using namespace std;
using namespace swss;

TEST(BinarySerializer, serialize_deserialize)
{
    string test_entry_key = "test_key";
    string test_command = "test_command";
    string test_key = "key";
    string test_value= "value";

    char buffer[100];
    BinarySerializer serializer(buffer, sizeof(buffer));
    std::vector<FieldValueTuple> values;
    values.push_back(std::make_pair(test_key, test_value));
    int serialized_len = (int)serializer.serializeBuffer(test_entry_key, values, test_command);
    string serialized_str(buffer);

    EXPECT_EQ(serialized_len, 72);

    auto ptr = std::make_shared<KeyOpFieldsValuesTuple>();
    KeyOpFieldsValuesTuple &kco = *ptr;
    auto& deserialized_values = kfvFieldsValues(kco);
    BinarySerializer::deSerializeBuffer(buffer, serialized_len, deserialized_values);
    
    swss::FieldValueTuple fvt = deserialized_values.at(0);
    EXPECT_TRUE(fvField(fvt) == test_entry_key);
    EXPECT_TRUE(fvValue(fvt) == test_command);

    fvt = deserialized_values.at(1);
    EXPECT_TRUE(fvField(fvt) == test_key);
    EXPECT_TRUE(fvValue(fvt) == test_value);
}

TEST(BinarySerializer, serialize_overflow)
{
    char buffer[50];
    BinarySerializer serializer(buffer, sizeof(buffer));
    std::vector<FieldValueTuple> values;
    values.push_back(std::make_pair("test_key", "test_value"));
    EXPECT_THROW(serializer.serializeBuffer("test_entry_key", values, "test_command"), runtime_error);
}

TEST(BinarySerializer, deserialize_overflow)
{
    char buffer[100];
    BinarySerializer serializer(buffer, sizeof(buffer));
    std::vector<FieldValueTuple> values;
    values.push_back(std::make_pair("test_key", "test_value"));
    int serialized_len = (int)serializer.serializeBuffer("test_entry_key", values, "test_command");
    string serialized_str(buffer);

    auto ptr = std::make_shared<KeyOpFieldsValuesTuple>();
    KeyOpFieldsValuesTuple &kco = *ptr;
    auto& deserialized_values = kfvFieldsValues(kco);
    EXPECT_THROW(BinarySerializer::deSerializeBuffer(buffer, serialized_len - 10, deserialized_values), runtime_error);
}
