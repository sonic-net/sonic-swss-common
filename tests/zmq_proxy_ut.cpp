#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include <deque>
#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/select.h"
#include "common/selectableevent.h"
#include "common/table.h"
#include "common/zmqserver.h"
#include "common/zmqclient.h"
#include "common/zmqproducerstatetable.h"
#include "common/zmqconsumerstatetable.h"

using namespace std;
using namespace swss;

#define TEST_DB               "APPL_DB"
#define TEST_TABLE_FOR_SERVER "TEST_TABLE_PROXY_FORWARD"
#define TEST_TABLE_FOR_PROXY  "TEST_TABLE_PROXY_CONSUME"

#define PROXY_ADDR      "tcp://*:5001"
#define PROXY_ENDPOINT  "tcp://localhost:5001"
#define SERVER_ADDR     "tcp://*:5002"
#define SERVER_ENDPOINT "tcp://localhost:5002"

#define SELECT_TIMEOUT_EXPECT_RECEIVE 10000
#define SELECT_TIMEOUT_EXPECT_NO_DATA  1000

std::vector<KeyOpFieldsValuesTuple> consume(ZmqConsumerStateTable &c, size_t keys_to_receive = 1)
{
    Select cs;
    cs.addSelectable(&c);

    Selectable *selectcs;
    std::vector<KeyOpFieldsValuesTuple> received;

    while (received.size() < keys_to_receive)
    {
        std::deque<KeyOpFieldsValuesTuple> vkco;
        int ret = cs.select(&selectcs, SELECT_TIMEOUT_EXPECT_RECEIVE, true);
        EXPECT_EQ(ret, Select::OBJECT);

        c.pops(vkco);
        received.insert(end(received), begin(vkco), end(vkco));
    }

    EXPECT_EQ(received.size(), keys_to_receive) << "Unexpected number of keys received";

    // Verify that all data are read
    int ret = cs.select(&selectcs, SELECT_TIMEOUT_EXPECT_NO_DATA);
    EXPECT_EQ(ret, Select::TIMEOUT) << "Unexpected data received in the consumer";

    return received;
}

void produce(ZmqClient &client, const string& table, const std::vector<KeyOpFieldsValuesTuple>& vkco)
{
    DBConnector db(TEST_DB, 0, true);
    ZmqProducerStateTable p(&db, table, client, false);
    p.set(vkco);
}

std::vector<KeyOpFieldsValuesTuple> generate_vkco(size_t keys_count = 1, const string& key_prefix = "")
{
    std::vector<KeyOpFieldsValuesTuple> data;

    for (size_t i = 0; i < keys_count; i++)
    {
        data.emplace_back(KeyOpFieldsValuesTuple{key_prefix + "test_key_" + to_string(i), "SET", std::vector<FieldValueTuple> {
            FieldValueTuple("test_field0", "test_value0"),
            FieldValueTuple("test_field1", "test_value1")
        }});
    }

    return data;
}

void validate_vkco(const std::vector<KeyOpFieldsValuesTuple>& vkco, size_t expected_keys_count = 1, const string& key_prefix = "")
{
    ASSERT_EQ(vkco.size(), expected_keys_count);

    for (size_t i = 0; i < expected_keys_count; i++)
    {
        ASSERT_EQ(kfvKey(vkco[i]), key_prefix + "test_key_" + to_string(i));
    }
}

TEST(ZmqProxy, proxy_forward)
{
    const size_t key_to_test = 5;
    std::vector<KeyOpFieldsValuesTuple> data = generate_vkco(key_to_test);
    DBConnector db(TEST_DB, 0, true);

    // Setup proxy server w/o any consumers to forward all
    ZmqServer proxyServer(PROXY_ADDR);
    proxyServer.enableProxyMode(SERVER_ENDPOINT);
    ASSERT_TRUE(proxyServer.isProxyMode());

    ZmqServer server(SERVER_ADDR);
    ASSERT_FALSE(server.isProxyMode());
    ZmqConsumerStateTable serverConsumer(&db, TEST_TABLE_FOR_SERVER, server);

    ZmqClient client(PROXY_ENDPOINT);
    produce(client, TEST_TABLE_FOR_SERVER, data);

    auto recevied = consume(serverConsumer, key_to_test);
    validate_vkco(recevied, key_to_test);
}

TEST(ZmqProxy, proxy_forward_consume)
{
    const size_t key_to_test = 5;
    std::vector<KeyOpFieldsValuesTuple> foward_data = generate_vkco(key_to_test, "forward");
    std::vector<KeyOpFieldsValuesTuple> consume_data = generate_vkco(key_to_test, "consume");
    DBConnector db(TEST_DB, 0, true);

    // Setup proxy server with a consumer for TEST_TABLE_FOR_PROXY table
    ZmqServer proxyServer(PROXY_ADDR);
    proxyServer.enableProxyMode(SERVER_ENDPOINT);
    ZmqConsumerStateTable proxyConsumer(&db, TEST_TABLE_FOR_PROXY, proxyServer);

    ZmqServer server(SERVER_ADDR);
    ZmqConsumerStateTable serverConsumer(&db, TEST_TABLE_FOR_SERVER, server);
    // This should not receive any data since the table is consumed in the proxy
    ZmqConsumerStateTable serverConsumerUnexpectedTable(&db, TEST_TABLE_FOR_PROXY, server);

    ZmqClient client(PROXY_ENDPOINT);
    produce(client, TEST_TABLE_FOR_SERVER, foward_data);
    produce(client, TEST_TABLE_FOR_PROXY, consume_data);

    auto recevied = consume(serverConsumer, key_to_test);
    validate_vkco(recevied, key_to_test, "forward");

    consume(serverConsumerUnexpectedTable, 0);

    recevied = consume(proxyConsumer, key_to_test);
    validate_vkco(recevied, key_to_test, "consume");
}
