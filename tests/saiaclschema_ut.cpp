#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

#include "common/saiaclschema.h"

namespace swss
{
namespace acl
{
namespace
{

using ::testing::AllOf;
using ::testing::Field;
using ::testing::UnorderedElementsAre;

class FormatTest : public testing::TestWithParam<Format>
{
};

TEST_P(FormatTest, StringConversionIsConsistent)
{
    EXPECT_EQ(FormatFromName(FormatName(GetParam())), GetParam());
}

INSTANTIATE_TEST_CASE_P(SaiAclSchemaTest, FormatTest,
                        testing::Values(Format::kNone, Format::kHexString, Format::kMac, Format::kIPv4, Format::kIPv6,
                                        Format::kString),
                        [](const testing::TestParamInfo<FormatTest::ParamType> &param_info) {
                            return FormatName(param_info.param);
                        });

class StageTest : public testing::TestWithParam<Stage>
{
};

TEST_P(StageTest, StringConversionIsConsistent)
{
    EXPECT_EQ(StageFromName(StageName(GetParam())), GetParam());
}

INSTANTIATE_TEST_CASE_P(SaiAclSchemaTest, StageTest,
                        testing::Values(Stage::kPreIngress, Stage::kIngress, Stage::kEgress),
                        [](const testing::TestParamInfo<StageTest::ParamType> &param_info) {
                            return StageName(param_info.param);
                        });

TEST(SaiAclSchemaTest, MatchFieldSchemaByNameSucceeds)
{
    EXPECT_THAT(MatchFieldSchemaByName("SAI_ACL_TABLE_ATTR_FIELD_SRC_IPV6"),
                AllOf(Field(&MatchFieldSchema::stages,
                            UnorderedElementsAre(Stage::kPreIngress, Stage::kIngress, Stage::kEgress)),
                      Field(&MatchFieldSchema::format, Format::kIPv6), Field(&MatchFieldSchema::bitwidth, 128)));
}

TEST(SaiAclSchemaTest, ActionSchemaByNameSucceeds)
{
    EXPECT_THAT(ActionSchemaByName("SAI_ACL_ENTRY_ATTR_ACTION_SET_INNER_VLAN_ID"),
                AllOf(Field(&ActionSchema::format, Format::kHexString), Field(&ActionSchema::bitwidth, 12)));
}

// Invalid Lookup Tests

TEST(SaiAclSchemaTest, InvalidFormatNameThrowsException)
{
    EXPECT_THROW(FormatFromName("Foo"), std::invalid_argument);
}

TEST(SaiAclSchemaTest, InvalidStageNameThrowsException)
{
    EXPECT_THROW(StageFromName("Foo"), std::invalid_argument);
}

TEST(SaiAclSchemaTest, InvalidMatchFieldNameThrowsException)
{
    EXPECT_THROW(MatchFieldSchemaByName("Foo"), std::invalid_argument);
}

TEST(SaiAclSchemaTest, InvalidActionNameThrowsException)
{
    EXPECT_THROW(ActionSchemaByName("Foo"), std::invalid_argument);
}

} // namespace
} // namespace acl
} // namespace swss
