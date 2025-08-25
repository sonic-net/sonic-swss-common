#include <vector>
#include <string>
#include <memory>
#include <cstdio>
#include <boost/format.hpp>
#include "gtest/gtest.h"
#include <nlohmann/json.hpp>

#include "common/dbconnector.h"
#include "common/table.h"

using namespace std;
using namespace swss;
using namespace boost;
using json = nlohmann::json;

struct file_deleter {
    void operator()(FILE* f) const {
        pclose(f);
    }
};

static void TestDPUDatabase(DBConnector &db)
{
    RedisReply r(&db, "FLUSHALL", REDIS_REPLY_STATUS);

    Table t(&db, "DASH_ENI_TABLE");
    vector<FieldValueTuple> values = {
        {"dashfield1", "dashvalue1"},
        {"dashfield2", "dashvalue2"}};

    t.set("dputest1", values);
    t.set("dputest2", values);

    vector<string> keys;
    t.getKeys(keys);
    EXPECT_EQ(keys.size(), (size_t)2);

    format fmt("redis-cli -n %1% -p %2% -h %3% hget 'DASH_ENI_TABLE:dputest1' dashfield1");
    SonicDBKey key = db.getDBKey();
    const std::string dbname = db.getDbName();
    std::string command = str(
        fmt % SonicDBConfig::getDbId(dbname, key) % SonicDBConfig::getDbPort(dbname, key) % SonicDBConfig::getDbHostname(dbname, key));
    std::unique_ptr<FILE, file_deleter> pipe(popen(command.c_str(), "r"));
    ASSERT_TRUE(pipe);
    char buffer[128] = {0};
    EXPECT_TRUE(fgets(buffer, sizeof(buffer), pipe.get()));
    EXPECT_STREQ(buffer, "dashvalue1\n");
}

TEST(DBConnector, access_dpu_db_from_npu)
{
    SonicDBKey key;
    key.containerName = "dpu0";
    DBConnector db("DPU_APPL_DB", 0, false, key);
    TestDPUDatabase(db);
}

TEST(DBConnector, access_dpu_db_from_dpu)
{
    SonicDBKey key;
    key.containerName = "dpu1";
    DBConnector db("DPU_APPL_DB", 0, true, key);
    TestDPUDatabase(db);
}
