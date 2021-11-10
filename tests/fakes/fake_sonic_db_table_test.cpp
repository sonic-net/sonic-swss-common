#include "swss/fakes/fake_sonic_db_table.h"

#include "absl/status/status.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace swss
{
namespace
{

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(FakeSonicDbTest, InsertSingleEntry)
{
    FakeSonicDbTable table;
    table.InsertTableEntry("entry", /*values=*/{});
    EXPECT_THAT(table.GetAllKeys(), ElementsAre("entry"));
}

TEST(FakeSonicDbTest, InsertDuplicateEntry)
{
    FakeSonicDbTable table;
    table.InsertTableEntry("entry", /*values=*/{});
    table.InsertTableEntry("entry", /*values=*/{});
    EXPECT_THAT(table.GetAllKeys(), ElementsAre("entry"));
}

TEST(FakeSonicDbTest, InsertMultipleEntries)
{
    FakeSonicDbTable table;
    table.InsertTableEntry("entry0", /*values=*/{});
    table.InsertTableEntry("entry1", /*values=*/{});
    EXPECT_THAT(table.GetAllKeys(), UnorderedElementsAre("entry0", "entry1"));
}

TEST(FakeSonicDbTest, ReadNonExistantEntryReturnsNotFoundError)
{
    FakeSonicDbTable table;
    EXPECT_EQ(table.ReadTableEntry("bad_entry").status().code(), absl::StatusCode::kNotFound);
}

TEST(FakeSonicDbTest, ReadBackEntry)
{
    FakeSonicDbTable table;
    table.InsertTableEntry("entry", /*values=*/{{"key", "value"}});

    auto result = table.ReadTableEntry("entry");
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(*result, UnorderedElementsAre(std::make_pair("key", "value")));
}

TEST(FakeSonicDbTest, InsertDuplicateKeyOverwritesExistingEntry)
{
    FakeSonicDbTable table;

    // First insert.
    table.InsertTableEntry("entry", /*values=*/{{"key", "value"}});
    auto result = table.ReadTableEntry("entry");
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(*result, UnorderedElementsAre(std::make_pair("key", "value")));

    // Second insert.
    table.InsertTableEntry("entry", /*values=*/{{"new_key", "new_value"}});
    result = table.ReadTableEntry("entry");
    ASSERT_TRUE(result.ok());
    EXPECT_THAT(*result, UnorderedElementsAre(std::make_pair("new_key", "new_value")));
}

TEST(FakeSonicDbTest, DeleteNonExistantEntry)
{
    // Is acts as a no-op.
    FakeSonicDbTable table;
    table.DeleteTableEntry("entry");
}

TEST(FakeSonicDbTest, DeleteEntry)
{
    FakeSonicDbTable table;

    // First insert.
    table.InsertTableEntry("entry", /*values=*/{{"key", "value"}});
    EXPECT_THAT(table.GetAllKeys(), ElementsAre("entry"));

    // Then delete.
    table.DeleteTableEntry("entry");
    EXPECT_TRUE(table.GetAllKeys().empty());
}

TEST(FakeSonicDbDeathTest, GetNotificationDiesIfNoNotificationExists)
{
    FakeSonicDbTable table;
    std::string op;
    std::string data;
    SonicDbEntry value;

    EXPECT_DEATH(table.GetNextNotification(op, data, value), "Could not find a notification");
}

TEST(FakeSonicDbTest, DefaultNotificationResponseIsSuccess)
{
    FakeSonicDbTable table;
    std::string op;
    std::string data;
    SonicDbEntry values;

    // First insert.
    table.InsertTableEntry("entry", /*values=*/{});
    table.GetNextNotification(op, data, values);
    EXPECT_EQ(op, "SWSS_RC_SUCCESS");
    EXPECT_EQ(data, "entry");
    EXPECT_THAT(values, ElementsAre(SonicDbKeyValue{"err_str", "SWSS_RC_SUCCESS"}));

    // Between actions clear the `values` container.
    values.clear();

    // Then delete.
    table.DeleteTableEntry("entry");
    table.GetNextNotification(op, data, values);
    EXPECT_EQ(op, "SWSS_RC_SUCCESS");
    EXPECT_EQ(data, "entry");
    EXPECT_THAT(values, ElementsAre(SonicDbKeyValue{"err_str", "SWSS_RC_SUCCESS"}));
}

TEST(FakeSonicDbTest, SetNotificationResponseForKey)
{
    FakeSonicDbTable table;
    std::string op;
    std::string data;
    SonicDbEntry values;

    table.SetResponseForKey(/*key=*/"entry", /*code=*/"SWSS_RC_UNKNOWN",
                            /*message=*/"my error message");
    table.InsertTableEntry("entry", /*values=*/{});
    table.GetNextNotification(op, data, values);
    EXPECT_EQ(op, "SWSS_RC_UNKNOWN");
    EXPECT_EQ(data, "entry");
    EXPECT_THAT(values, ElementsAre(SonicDbKeyValue{"err_str", "my error message"}));
}

TEST(FakeSonicDbTest, DebutState)
{
    // This is a no-op.
    FakeSonicDbTable table;
    table.InsertTableEntry("entry", /*values=*/{{"key", "value"}});
    table.DebugState();
}

} // namespace
} // namespace swss

int main(int argc, char **argv)
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
