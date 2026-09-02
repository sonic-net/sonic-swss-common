#include "gtest/gtest.h"

#include "common/interface.h"

TEST(InterfaceName, AcceptsCurrentCallerNames)
{
    EXPECT_TRUE(swss::isInterfaceNameValid("PortChannel0001"));
    EXPECT_TRUE(swss::isInterfaceNameValid("Vrf-RED"));
    EXPECT_TRUE(swss::isInterfaceNameValid("vtep1"));
    EXPECT_TRUE(swss::isInterfaceNameValid("Ethernet0.100"));
}

TEST(InterfaceName, AcceptsSharedSyntax)
{
    EXPECT_TRUE(swss::isInterfaceNameValid("Ethernet0"));
    EXPECT_TRUE(swss::isInterfaceNameValid("PortChannel1"));
    EXPECT_TRUE(swss::isInterfaceNameValid("Eth0.100"));
    EXPECT_TRUE(swss::isInterfaceNameValid("Ethernet-BP0"));
    EXPECT_TRUE(swss::isInterfaceNameValid("1Ethernet"));
    EXPECT_TRUE(swss::isInterfaceNameValid("_eth0"));
    EXPECT_TRUE(swss::isInterfaceNameValid(".eth0"));
    EXPECT_TRUE(swss::isInterfaceNameValid(std::string(swss::IFACE_NAME_MAX_LEN, 'a')));
}

TEST(InterfaceName, RejectsUnsupportedSyntax)
{
    EXPECT_FALSE(swss::isInterfaceNameValid(""));
    EXPECT_FALSE(swss::isInterfaceNameValid("."));
    EXPECT_FALSE(swss::isInterfaceNameValid(".."));
    EXPECT_FALSE(swss::isInterfaceNameValid("-Ethernet0"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Port Channel"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet0/1"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet0:1"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet0@1"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet;0"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet$0"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet`0"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet|0"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet&0"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet\\0"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet\n0"));
    EXPECT_FALSE(swss::isInterfaceNameValid("Ethernet\t0"));
    EXPECT_FALSE(swss::isInterfaceNameValid(std::string(IFNAMSIZ, 'a')));
}
