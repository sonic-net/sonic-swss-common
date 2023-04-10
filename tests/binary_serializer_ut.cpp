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
    string test_db = "test_db";
    string test_table = "test_table";
    string test_key = "key";
    string test_value= "value";

    char buffer[200];
    std::vector<FieldValueTuple> values;
    values.push_back(std::make_pair(test_key, test_value));
    int serialized_len = (int)BinarySerializer::serializeBuffer(
                                                                buffer,
                                                                sizeof(buffer),
                                                                test_entry_key,
                                                                values,
                                                                test_command,
                                                                test_db,
                                                                test_table);
    string serialized_str(buffer);

    EXPECT_EQ(serialized_len, 107);

    auto ptr = std::make_shared<KeyOpFieldsValuesTuple>();
    KeyOpFieldsValuesTuple &kco = *ptr;
    auto& deserialized_values = kfvFieldsValues(kco);
    BinarySerializer::deserializeBuffer(buffer, serialized_len, deserialized_values);
    
    swss::FieldValueTuple fvt = deserialized_values.at(0);
    EXPECT_TRUE(fvField(fvt) == test_db);
    EXPECT_TRUE(fvValue(fvt) == test_table);

    fvt = deserialized_values.at(1);
    EXPECT_TRUE(fvField(fvt) == test_entry_key);
    EXPECT_TRUE(fvValue(fvt) == test_command);

    fvt = deserialized_values.at(2);
    EXPECT_TRUE(fvField(fvt) == test_key);
    EXPECT_TRUE(fvValue(fvt) == test_value);
}

TEST(BinarySerializer, serialize_overflow)
{
    char buffer[50];
    std::vector<FieldValueTuple> values;
    values.push_back(std::make_pair("test_key", "test_value"));
    EXPECT_THROW(BinarySerializer::serializeBuffer(
                                                buffer,
                                                sizeof(buffer),
                                                "test_entry_key",
                                                values,
                                                "test_command",
                                                "test_db",
                                                "test_table"), runtime_error);
}

TEST(BinarySerializer, deserialize_overflow)
{
    char buffer[200];
    std::vector<FieldValueTuple> values;
    values.push_back(std::make_pair("test_key", "test_value"));
    int serialized_len = (int)BinarySerializer::serializeBuffer(
                                                                buffer,
                                                                sizeof(buffer),
                                                                "test_entry_key",
                                                                values,
                                                                "test_command",
                                                                "test_db",
                                                                "test_table");
    string serialized_str(buffer);

    auto ptr = std::make_shared<KeyOpFieldsValuesTuple>();
    KeyOpFieldsValuesTuple &kco = *ptr;
    auto& deserialized_values = kfvFieldsValues(kco);
    EXPECT_THROW(BinarySerializer::deserializeBuffer(buffer, serialized_len - 10, deserialized_values), runtime_error);
}
