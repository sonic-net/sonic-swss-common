#include <iostream>
#include "gtest/gtest.h"

#include "common/table.h"
#include "common/binaryserializer.h"

using namespace std;
using namespace swss;

TEST(BinarySerializer, serialize_deserialize)
{
    string test_entry_key = "test_key";
    string test_command = "SET";
    string test_db = "test_db";
    string test_table = "test_table";
    string test_key = "key";
    string test_value= "value";
    string test_entry_key2 = "test_key_2";
    string test_command2 = "DEL";

    char buffer[200];
    std::vector<FieldValueTuple> values;
    values.push_back(std::make_pair(test_key, test_value));
    std::vector<KeyOpFieldsValuesTuple> kcos = std::vector<KeyOpFieldsValuesTuple>{
        KeyOpFieldsValuesTuple{test_entry_key, test_command, values},
        KeyOpFieldsValuesTuple{test_entry_key2, test_command2, std::vector<FieldValueTuple>{}}};
    int serialized_len = (int)BinarySerializer::serializeBuffer(
                                                                buffer,
                                                                sizeof(buffer),
                                                                test_db,
                                                                test_table,
                                                                kcos);
    string serialized_str(buffer);

    EXPECT_EQ(serialized_len, 117);

    std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> kcos_ptrs;
    std::vector<KeyOpFieldsValuesTuple> deserialized_kcos;
    string db_name;
    string db_table;
    BinarySerializer::deserializeBuffer(buffer, serialized_len, db_name, db_table, kcos_ptrs);
    for (auto kco_ptr : kcos_ptrs)
    {
        deserialized_kcos.push_back(*kco_ptr);
    }

    EXPECT_EQ(db_name, test_db);
    EXPECT_EQ(db_table, test_table);
    EXPECT_EQ(deserialized_kcos, kcos);
}

TEST(BinarySerializer, serialize_overflow)
{
    char buffer[50];
    std::vector<FieldValueTuple> values;
    values.push_back(std::make_pair("test_key", "test_value"));
    std::vector<KeyOpFieldsValuesTuple> kcos = std::vector<KeyOpFieldsValuesTuple>{
        KeyOpFieldsValuesTuple{"test_entry_key", "SET", values}};
    EXPECT_THROW(BinarySerializer::serializeBuffer(
                                                buffer,
                                                sizeof(buffer),
                                                "test_db",
                                                "test_table",
                                                kcos), runtime_error);
}

TEST(BinarySerializer, deserialize_overflow)
{
    char buffer[200];
    std::vector<FieldValueTuple> values;
    values.push_back(std::make_pair("test_key", "test_value"));
    std::vector<KeyOpFieldsValuesTuple> kcos = std::vector<KeyOpFieldsValuesTuple>{
        KeyOpFieldsValuesTuple{"test_entry_key", "SET", values}};
    int serialized_len = (int)BinarySerializer::serializeBuffer(
                                                                buffer,
                                                                sizeof(buffer),
                                                                "test_db",
                                                                "test_table",
                                                                kcos);
    string serialized_str(buffer);

    std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> kcos_ptrs;
    string db_name;
    string db_table;
    EXPECT_THROW(BinarySerializer::deserializeBuffer(buffer, serialized_len - 10, db_name, db_table, kcos_ptrs), runtime_error);
}

TEST(BinarySerializer, protocol_buffer)
{
    string test_entry_key = "test_key";
    string test_command = "SET";
    string test_db = "test_db";
    string test_table = "test_table";
    string test_key = "key";
    unsigned char binary_proto_buf[] = {0x0a, 0x05, 0x0d, 0x0a, 0x01, 0x00, 0x20, 0x10, 0xe1, 0x21};
    string proto_buf_val = string((const char *)binary_proto_buf, sizeof(binary_proto_buf));
    EXPECT_TRUE(proto_buf_val.length() == sizeof(binary_proto_buf));

    char buffer[200];
    std::vector<FieldValueTuple> values;
    values.push_back(std::make_pair(test_key, proto_buf_val));
    std::vector<KeyOpFieldsValuesTuple> kcos = std::vector<KeyOpFieldsValuesTuple>{
        KeyOpFieldsValuesTuple{test_entry_key, test_command, values}};
    int serialized_len = (int)BinarySerializer::serializeBuffer(
                                                                buffer,
                                                                sizeof(buffer),
                                                                test_db,
                                                                test_table,
                                                                kcos);
    string serialized_str(buffer);

    EXPECT_EQ(serialized_len, 95);

    std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> kcos_ptrs;
    std::vector<KeyOpFieldsValuesTuple> deserialized_kcos;
    string db_name;
    string db_table;
    BinarySerializer::deserializeBuffer(buffer, serialized_len, db_name, db_table, kcos_ptrs);
    for (auto kco_ptr : kcos_ptrs)
    {
        deserialized_kcos.push_back(*kco_ptr);
    }

    EXPECT_EQ(db_name, test_db);
    EXPECT_EQ(db_table, test_table);
    EXPECT_EQ(deserialized_kcos, kcos);
}
