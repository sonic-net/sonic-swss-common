#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>

#include <gtest/gtest.h>
#include "redisapi.h"
#include "rules_cc/cc/runfiles/runfiles.h"

namespace {
std::string fixture;

class ScopedLuaDirectory {
public:
    explicit ScopedLuaDirectory(const char *value)
    {
        const char *old = std::getenv("SWSS_COMMON_LUA_DIR");
        hadPrevious = old != nullptr;
        previous = old ? old : "";
        const int result = value ? setenv("SWSS_COMMON_LUA_DIR", value, 1)
                                 : unsetenv("SWSS_COMMON_LUA_DIR");
        if (result != 0) throw std::runtime_error("Cannot configure Lua test directory");
    }
    ~ScopedLuaDirectory()
    {
        if (hadPrevious) setenv("SWSS_COMMON_LUA_DIR", previous.c_str(), 1);
        else unsetenv("SWSS_COMMON_LUA_DIR");
    }
private:
    bool hadPrevious;
    std::string previous;
};

void expectMissingAt(const std::string &directory, const std::string &name)
{
    try {
        swss::loadLuaScript(name);
        FAIL() << "A missing declared Lua script must not silently succeed";
    } catch (const std::runtime_error &error) {
        EXPECT_NE(std::string(error.what()).find(directory + "/" + name), std::string::npos);
    }
}

TEST(RedisLuaPath, ReadsActualOwnerScript)
{
    const auto slash = fixture.find_last_of('/');
    ASSERT_NE(slash, std::string::npos);
    const std::string directory = fixture.substr(0, slash);
    ScopedLuaDirectory scope(directory.c_str());
    std::ifstream input(fixture);
    ASSERT_TRUE(input.good());
    const std::string expected((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    ASSERT_FALSE(expected.empty());
    EXPECT_EQ(swss::loadLuaScript(fixture.substr(slash + 1)), expected);
}

TEST(RedisLuaPath, MissingDeclaredScriptDoesNotFallBack)
{
    const std::string directory = fixture.substr(0, fixture.find_last_of('/'));
    ScopedLuaDirectory scope(directory.c_str());
    expectMissingAt(directory, ".sonic-bazel-absent-" + std::to_string(getpid()) + ".lua");
}

TEST(RedisLuaPath, OnlyAnOverrideRestrictsNestedScriptNames)
{
    const std::string name = ".sonic-bazel-absent-" + std::to_string(getpid()) + "/script.lua";
    { ScopedLuaDirectory scope(nullptr); expectMissingAt("/usr/share/swss", name); }
    { ScopedLuaDirectory scope(""); expectMissingAt("/usr/share/swss", name); }

    const std::string directory = fixture.substr(0, fixture.find_last_of('/'));
    ScopedLuaDirectory scope(directory.c_str());
    EXPECT_THROW(swss::loadLuaScript(name), std::invalid_argument);
}
TEST(RedisLuaPath, RejectsScriptTraversal)
{
    const std::string directory = fixture.substr(0, fixture.find_last_of('/'));
    ScopedLuaDirectory scope(directory.c_str());
    for (const std::string name : {"../redisapi.h", "/etc/passwd", "nested/file.lua", "nested\\file.lua", ".", "..", ""})
    {
        EXPECT_THROW(swss::loadLuaScript(name), std::invalid_argument) << name;
    }
}

TEST(RedisLuaPath, RejectsRelativeDirectory)
{
    ScopedLuaDirectory scope("common");
    EXPECT_THROW(swss::loadLuaScript("producer_state_table_apply_view.lua"), std::invalid_argument);
}

TEST(RedisLuaPath, RejectsSymlinkOutsideConfiguredRoot)
{
    char pattern[] = "/tmp/swss-lua-path-XXXXXX";
    const char *created = mkdtemp(pattern);
    ASSERT_NE(created, nullptr);
    const std::string root = created;
    const std::string link = root + "/escape.lua";
    struct Cleanup {
        std::string link, root;
        ~Cleanup() { unlink(link.c_str()); rmdir(root.c_str()); }
    } cleanup{link, root};
    ASSERT_EQ(symlink(fixture.c_str(), link.c_str()), 0);
    ScopedLuaDirectory scope(root.c_str());
    EXPECT_THROW(swss::loadLuaScript("escape.lua"), std::invalid_argument);
}

} // namespace

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (argc != 1) return 2;
    std::string error;
    std::unique_ptr<rules_cc::cc::runfiles::Runfiles> runfiles(
        rules_cc::cc::runfiles::Runfiles::CreateForTest(BAZEL_CURRENT_REPOSITORY, &error));
    if (!runfiles) throw std::runtime_error(error);
    const std::string source = runfiles->Rlocation(
        "sonic-swss-common/common/producer_state_table_apply_view.lua");
    std::unique_ptr<char, decltype(&std::free)> resolved(realpath(source.c_str(), nullptr), std::free);
    if (!resolved) throw std::runtime_error("Cannot resolve declared owner Lua fixture");
    fixture = resolved.get();
    return RUN_ALL_TESTS();
}
