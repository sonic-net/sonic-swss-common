#include "gtest/gtest.h"

#include "common/vrf.h"

TEST(VrfName, AcceptsCurrentNames)
{
    EXPECT_TRUE(swss::isVrfNameValid("default"));
    EXPECT_TRUE(swss::isVrfNameValid("mgmt"));
    EXPECT_TRUE(swss::isVrfNameValid("vrfRED"));
    EXPECT_TRUE(swss::isVrfNameValid("Vrf-RED_1"));
    EXPECT_TRUE(swss::isVrfNameValid("Vrf.RED"));
    EXPECT_TRUE(swss::isVrfNameValid(std::string(swss::VRF_NAME_MAX_LEN, 'a')));
}

TEST(VrfName, RejectsUnsupportedSyntax)
{
    EXPECT_FALSE(swss::isVrfNameValid(""));
    EXPECT_FALSE(swss::isVrfNameValid("."));
    EXPECT_FALSE(swss::isVrfNameValid(".."));
    EXPECT_FALSE(swss::isVrfNameValid("-VrfRED"));
    EXPECT_FALSE(swss::isVrfNameValid("Vrf RED"));
    EXPECT_FALSE(swss::isVrfNameValid("Vrf/RED"));
    EXPECT_FALSE(swss::isVrfNameValid("Vrf:RED"));
    EXPECT_FALSE(swss::isVrfNameValid("Vrf|RED"));
    EXPECT_FALSE(swss::isVrfNameValid(std::string(IFNAMSIZ, 'a')));
}
