#include "../common/c-api/sonicv2connector.h"
#include "../common/c-api/util.h"

#include <gtest/gtest.h>
#include <cstring>
#include <string>

namespace
{

class SonicV2ConnectorCApiTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a new SonicV2Connector instance for each test
        result = SWSSSonicV2Connector_new(1, nullptr, &connector);
        ASSERT_EQ(result.exception, SWSSException_None);
        ASSERT_NE(connector, nullptr);
    }

    void TearDown() override
    {
        // Clean up the connector
        if (connector != nullptr) {
            SWSSSonicV2Connector_free(connector);
            connector = nullptr;
        }
    }

    SWSSSonicV2Connector connector = nullptr;
    SWSSResult result;
};

TEST_F(SonicV2ConnectorCApiTest, CreateAndFreeConnector)
{
    // Test creating a new connector
    SWSSSonicV2Connector test_connector;
    SWSSResult test_result = SWSSSonicV2Connector_new(1, nullptr, &test_connector);
    EXPECT_EQ(test_result.exception, SWSSException_None);
    EXPECT_NE(test_connector, nullptr);

    // Test freeing the connector
    test_result = SWSSSonicV2Connector_free(test_connector);
    EXPECT_EQ(test_result.exception, SWSSException_None);
}

TEST_F(SonicV2ConnectorCApiTest, CreateConnectorWithNamespace)
{
    // Test creating connector with namespace
    SWSSSonicV2Connector test_connector;
    const char* test_namespace = "test_namespace";
    SWSSResult test_result = SWSSSonicV2Connector_new(1, test_namespace, &test_connector);
    EXPECT_EQ(test_result.exception, SWSSException_None);
    EXPECT_NE(test_connector, nullptr);

    // Get namespace and verify
    SWSSString namespace_str;
    test_result = SWSSSonicV2Connector_getNamespace(test_connector, &namespace_str);
    EXPECT_EQ(test_result.exception, SWSSException_None);

    SWSSStrRef namespace_ref = (SWSSStrRef)namespace_str;
    const char* returned_namespace = SWSSStrRef_c_str(namespace_ref);
    EXPECT_STREQ(returned_namespace, test_namespace);

    // Clean up
    SWSSString_free(namespace_str);
    SWSSSonicV2Connector_free(test_connector);
}

TEST_F(SonicV2ConnectorCApiTest, GetNamespace)
{
    // Test getting namespace (should be empty for default)
    SWSSString namespace_str;
    result = SWSSSonicV2Connector_getNamespace(connector, &namespace_str);
    EXPECT_EQ(result.exception, SWSSException_None);

    SWSSStrRef namespace_ref = (SWSSStrRef)namespace_str;
    const char* returned_namespace = SWSSStrRef_c_str(namespace_ref);
    EXPECT_STREQ(returned_namespace, "");

    SWSSString_free(namespace_str);
}

TEST_F(SonicV2ConnectorCApiTest, ConnectAndClose)
{
    // Test connecting to a database
    result = SWSSSonicV2Connector_connect(connector, "TEST_DB", 1);
    EXPECT_EQ(result.exception, SWSSException_None);

    // Test closing specific database connection
    result = SWSSSonicV2Connector_close_db(connector, "TEST_DB");
    EXPECT_EQ(result.exception, SWSSException_None);

    // Test closing all connections
    result = SWSSSonicV2Connector_close_all(connector);
    EXPECT_EQ(result.exception, SWSSException_None);
}

TEST_F(SonicV2ConnectorCApiTest, GetDbList)
{
    // Test getting database list
    SWSSStringArray db_list;
    result = SWSSSonicV2Connector_get_db_list(connector, &db_list);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_GT(db_list.len, 0);

    // Clean up
    for (uint64_t i = 0; i < db_list.len; i++) {
        free(const_cast<char*>(db_list.data[i]));
    }
    SWSSStringArray_free(db_list);
}

TEST_F(SonicV2ConnectorCApiTest, GetDbId)
{
    // Test getting database ID
    int db_id;
    result = SWSSSonicV2Connector_get_dbid(connector, "TEST_DB", &db_id);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_GE(db_id, 0);
}

TEST_F(SonicV2ConnectorCApiTest, GetDbSeparator)
{
    // Test getting database separator
    SWSSString separator;
    result = SWSSSonicV2Connector_get_db_separator(connector, "TEST_DB", &separator);
    EXPECT_EQ(result.exception, SWSSException_None);

    SWSSStrRef separator_ref = (SWSSStrRef)separator;
    const char* sep_str = SWSSStrRef_c_str(separator_ref);
    EXPECT_NE(sep_str, nullptr);
    EXPECT_GT(strlen(sep_str), 0);

    SWSSString_free(separator);
}

TEST_F(SonicV2ConnectorCApiTest, BasicRedisOperations)
{
    // Connect to test database
    result = SWSSSonicV2Connector_connect(connector, "TEST_DB", 1);
    EXPECT_EQ(result.exception, SWSSException_None);

    // Test exists operation on non-existing key
    uint8_t exists;
    result = SWSSSonicV2Connector_exists(connector, "TEST_DB", "test_key", &exists);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_EQ(exists, 0);

    // Test set operation
    int64_t set_result;
    result = SWSSSonicV2Connector_set(connector, "TEST_DB", "test_key", "field1", "value1", 0, &set_result);
    EXPECT_EQ(result.exception, SWSSException_None);

    // Test exists operation on existing key
    result = SWSSSonicV2Connector_exists(connector, "TEST_DB", "test_key", &exists);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_EQ(exists, 1);

    // Test hexists operation
    uint8_t hexists;
    result = SWSSSonicV2Connector_hexists(connector, "TEST_DB", "test_key", "field1", &hexists);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_EQ(hexists, 1);

    // Test get operation
    SWSSString value;
    result = SWSSSonicV2Connector_get(connector, "TEST_DB", "test_key", "field1", 0, &value);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_NE(value, nullptr);

    SWSSStrRef value_ref = (SWSSStrRef)value;
    const char* value_str = SWSSStrRef_c_str(value_ref);
    EXPECT_STREQ(value_str, "value1");
    SWSSString_free(value);

    // Test get_all operation
    SWSSFieldValueArray field_values;
    result = SWSSSonicV2Connector_get_all(connector, "TEST_DB", "test_key", 0, &field_values);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_EQ(field_values.len, 1);
    EXPECT_STREQ(field_values.data[0].field, "field1");

    SWSSStrRef fv_value_ref = (SWSSStrRef)field_values.data[0].value;
    const char* fv_value_str = SWSSStrRef_c_str(fv_value_ref);
    EXPECT_STREQ(fv_value_str, "value1");

    // Clean up field values
    free(const_cast<char*>(field_values.data[0].field));
    SWSSString_free(field_values.data[0].value);
    SWSSFieldValueArray_free(field_values);

    // Test delete operation
    int64_t del_result;
    result = SWSSSonicV2Connector_del(connector, "TEST_DB", "test_key", 0, &del_result);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_EQ(del_result, 1);

    // Verify key is deleted
    result = SWSSSonicV2Connector_exists(connector, "TEST_DB", "test_key", &exists);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_EQ(exists, 0);
}

TEST_F(SonicV2ConnectorCApiTest, HmsetOperation)
{
    // Connect to test database
    result = SWSSSonicV2Connector_connect(connector, "TEST_DB", 1);
    EXPECT_EQ(result.exception, SWSSException_None);

    // Prepare field-value pairs for hmset
    SWSSFieldValueTuple fv_data[2];
    fv_data[0].field = strdup("field1");
    fv_data[0].value = SWSSString_new_c_str("value1");
    fv_data[1].field = strdup("field2");
    fv_data[1].value = SWSSString_new_c_str("value2");

    SWSSFieldValueArray fv_array;
    fv_array.len = 2;
    fv_array.data = fv_data;

    // Test hmset operation
    result = SWSSSonicV2Connector_hmset(connector, "TEST_DB", "test_hmset_key", &fv_array);
    EXPECT_EQ(result.exception, SWSSException_None);

    // Verify the data was set correctly
    SWSSFieldValueArray retrieved_values;
    result = SWSSSonicV2Connector_get_all(connector, "TEST_DB", "test_hmset_key", 0, &retrieved_values);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_EQ(retrieved_values.len, 2);

    // Clean up
    for (uint64_t i = 0; i < retrieved_values.len; i++) {
        free(const_cast<char*>(retrieved_values.data[i].field));
        SWSSString_free(retrieved_values.data[i].value);
    }
    SWSSFieldValueArray_free(retrieved_values);

    // Clean up input data
    for (int i = 0; i < 2; i++) {
        free(const_cast<char*>(fv_data[i].field));
        SWSSString_free(fv_data[i].value);
    }

    // Clean up the test key
    int64_t del_result;
    SWSSSonicV2Connector_del(connector, "TEST_DB", "test_hmset_key", 0, &del_result);
}

TEST_F(SonicV2ConnectorCApiTest, KeysOperation)
{
    // Connect to test database
    result = SWSSSonicV2Connector_connect(connector, "TEST_DB", 1);
    EXPECT_EQ(result.exception, SWSSException_None);

    // Set some test keys
    int64_t set_result;
    SWSSSonicV2Connector_set(connector, "TEST_DB", "test_key1", "field1", "value1", 0, &set_result);
    SWSSSonicV2Connector_set(connector, "TEST_DB", "test_key2", "field1", "value1", 0, &set_result);
    SWSSSonicV2Connector_set(connector, "TEST_DB", "other_key", "field1", "value1", 0, &set_result);

    // Test keys operation with pattern
    SWSSStringArray keys;
    result = SWSSSonicV2Connector_keys(connector, "TEST_DB", "test_*", 0, &keys);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_GE(keys.len, 2);

    // Clean up keys
    for (uint64_t i = 0; i < keys.len; i++) {
        free(const_cast<char*>(keys.data[i]));
    }
    SWSSStringArray_free(keys);

    // Clean up test keys
    int64_t del_result;
    SWSSSonicV2Connector_del(connector, "TEST_DB", "test_key1", 0, &del_result);
    SWSSSonicV2Connector_del(connector, "TEST_DB", "test_key2", 0, &del_result);
    SWSSSonicV2Connector_del(connector, "TEST_DB", "other_key", 0, &del_result);
}

TEST_F(SonicV2ConnectorCApiTest, ScanOperation)
{
    // Connect to test database
    result = SWSSSonicV2Connector_connect(connector, "TEST_DB", 1);
    EXPECT_EQ(result.exception, SWSSException_None);

    // Set some test keys
    int64_t set_result;
    SWSSSonicV2Connector_set(connector, "TEST_DB", "scan_key1", "field1", "value1", 0, &set_result);
    SWSSSonicV2Connector_set(connector, "TEST_DB", "scan_key2", "field1", "value1", 0, &set_result);

    // Test scan operation
    int cursor = 0;
    int new_cursor;
    SWSSStringArray keys;
    result = SWSSSonicV2Connector_scan(connector, "TEST_DB", cursor, "scan_*", 10, &new_cursor, &keys);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_GE(keys.len, 0);

    // Clean up keys
    for (uint64_t i = 0; i < keys.len; i++) {
        free(const_cast<char*>(keys.data[i]));
    }
    SWSSStringArray_free(keys);

    // Clean up test keys
    int64_t del_result;
    SWSSSonicV2Connector_del(connector, "TEST_DB", "scan_key1", 0, &del_result);
    SWSSSonicV2Connector_del(connector, "TEST_DB", "scan_key2", 0, &del_result);
}

TEST_F(SonicV2ConnectorCApiTest, PublishOperation)
{
    // Connect to test database
    result = SWSSSonicV2Connector_connect(connector, "TEST_DB", 1);
    EXPECT_EQ(result.exception, SWSSException_None);

    // Test publish operation
    int64_t publish_result;
    result = SWSSSonicV2Connector_publish(connector, "TEST_DB", "test_channel", "test_message", &publish_result);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_GE(publish_result, 0);
}

TEST_F(SonicV2ConnectorCApiTest, GetRedisClient)
{
    // Connect to test database
    result = SWSSSonicV2Connector_connect(connector, "TEST_DB", 1);
    EXPECT_EQ(result.exception, SWSSException_None);

    // Test getting Redis client
    SWSSDBConnector redis_client;
    result = SWSSSonicV2Connector_get_redis_client(connector, "TEST_DB", &redis_client);
    EXPECT_EQ(result.exception, SWSSException_None);
    EXPECT_NE(redis_client, nullptr);
}

TEST_F(SonicV2ConnectorCApiTest, DeleteAllByPattern)
{
    // Connect to test database
    result = SWSSSonicV2Connector_connect(connector, "TEST_DB", 1);
    EXPECT_EQ(result.exception, SWSSException_None);

    // Set some test keys
    int64_t set_result;
    SWSSSonicV2Connector_set(connector, "TEST_DB", "pattern_key1", "field1", "value1", 0, &set_result);
    SWSSSonicV2Connector_set(connector, "TEST_DB", "pattern_key2", "field1", "value1", 0, &set_result);
    SWSSSonicV2Connector_set(connector, "TEST_DB", "other_key", "field1", "value1", 0, &set_result);

    // Test delete all by pattern
    result = SWSSSonicV2Connector_delete_all_by_pattern(connector, "TEST_DB", "pattern_*");
    EXPECT_EQ(result.exception, SWSSException_None);

    // Verify pattern keys are deleted
    uint8_t exists;
    SWSSSonicV2Connector_exists(connector, "TEST_DB", "pattern_key1", &exists);
    EXPECT_EQ(exists, 0);
    SWSSSonicV2Connector_exists(connector, "TEST_DB", "pattern_key2", &exists);
    EXPECT_EQ(exists, 0);

    // Verify other key still exists
    SWSSSonicV2Connector_exists(connector, "TEST_DB", "other_key", &exists);
    EXPECT_EQ(exists, 1);

    // Clean up remaining key
    int64_t del_result;
    SWSSSonicV2Connector_del(connector, "TEST_DB", "other_key", 0, &del_result);
}

} // namespace