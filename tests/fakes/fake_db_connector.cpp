#include "swss/fakes/fake_db_connector.h"

#include <fnmatch.h>
#include <memory>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "glog/logging.h"

namespace swss
{
namespace
{

// When the DBConnector sees a key we expected it to be in the format:
//   <table_name>:<key>
struct RedisDbKey
{
    std::string table_name;
    std::string key;
};

// Parses a string as a RedisDB key. If the key is not in our expected format we
// return a status failure.
absl::StatusOr<RedisDbKey> GetRedisDbKey(const std::string &key, const std::string &delimiter)
{
    std::vector<std::string> split = absl::StrSplit(key, delimiter);

    // If there is no ':' character then the key is incorrectly formatted for our
    // use case.
    if (split.size() == 1)
    {
        return absl::Status(absl::StatusCode::kInvalidArgument,
                            absl::StrCat("Key does not have a '", delimiter, "': ", key));
    }
    return RedisDbKey{.table_name = split[0], .key = absl::StrJoin(split.begin() + 1, split.end(), delimiter)};
}

} // namespace

void FakeDBConnector::AddAppDbTable(const std::string &table_name, FakeSonicDbTable *table)
{
    sonic_db_tables_[table_name] = table;
}

// Redis KEYS patterns use the glob-style for regular expression matching.
std::vector<std::string> FakeDBConnector::keys(const std::string &glob)
{
    VLOG(1) << "Getting keys matching: " << glob;
    std::vector<std::string> result;

    // The fnmatch() function requires a char* for the regular expression.
    // Therefore, we need to create a local copy of glob.
    const std::string glob_str(glob);

    // Iterates through every AppDb table and compares the key against the `glob`
    // argument.
    for (const auto &table : sonic_db_tables_)
    {
        VLOG(2) << "Searching table: " << table.first;
        for (const std::string &key : table.second->GetAllKeys())
        {
            // When the DBConnector sees a key it is in the format:
            //   <table_name>:<key>
            const std::string full_key = absl::StrCat(table.first, ":", key);
            VLOG(2) << "Found key: " << full_key;

            // If fnmatch() finds a glob match it will return 0.
            if (fnmatch(glob_str.c_str(), full_key.c_str(), /*flags=*/0) == 0)
            {
                VLOG(3) << "Found match: " << full_key;
                result.push_back(full_key);
            }
        }
    }
    return result;
}

std::unordered_map<std::string, std::string> FakeDBConnector::hgetall(const std::string &key)
{
    VLOG(1) << "Getting data for key: " << key;
    std::unordered_map<std::string, std::string> result;

    // If we get an invalid key we assume the entry does not exist and return
    // an empty map..
    auto redis_key = GetRedisDbKey(key, delimiter_);
    if (!redis_key.ok())
    {
        VLOG(1) << "WARNING: " << redis_key.status();
        return result;
    }

    // If the fake doesn't have visibility into the table then we can't return
    // anything.
    auto app_db_table_iter = sonic_db_tables_.find(redis_key->table_name);
    if (app_db_table_iter == sonic_db_tables_.end())
    {
        VLOG(1) << "WARNING: Could not find AppDb table: " << redis_key->table_name;
        return result;
    }

    // Otherwise, we try to read the entry values from the table.
    auto entry_or = app_db_table_iter->second->ReadTableEntry(redis_key->key);
    if (!entry_or.ok())
    {
        VLOG(1) << "WARNING: Could not find AppDb entry: " << entry_or.status();
        return result;
    }

    // Convert the vector of pairs into a map before returning.
    for (const auto &data : *entry_or)
    {
        result[data.first] = data.second;
    }
    return result;
}

bool FakeDBConnector::exists(const std::string &key)
{
    VLOG(1) << "Checking if key exists: " << key;

    // If we get an invalid key we assume the entry does not exist and return
    // false.
    auto redis_key = GetRedisDbKey(key, delimiter_);
    if (!redis_key.ok())
    {
        VLOG(1) << "WARNING: " << redis_key.status();
        return false;
    }

    // If the fake doesn't have visibility into the table then we also return
    // false.
    auto app_db_table_iter = sonic_db_tables_.find(redis_key->table_name);
    if (app_db_table_iter == sonic_db_tables_.end())
    {
        VLOG(1) << "WARNING: Could not find AppDb table: " << redis_key->table_name;
        return false;
    }

    // Otherwise, we try to find the value. A status failure implies we could not
    // find the key.
    auto status = app_db_table_iter->second->ReadTableEntry(redis_key->key);
    return status.ok();
}

int64_t FakeDBConnector::del(const std::string &key)
{
    VLOG(1) << "Deleteing key: " << key;

    // If we get an invalid key we assume the entry does not exist and return 0.
    auto redis_key = GetRedisDbKey(key, delimiter_);
    if (!redis_key.ok())
    {
        VLOG(1) << "WARNING: " << redis_key.status();
        return 0;
    }

    // If the fake doesn't have visibility into the table then we also return 0.
    auto app_db_table_iter = sonic_db_tables_.find(redis_key->table_name);
    if (app_db_table_iter == sonic_db_tables_.end())
    {
        VLOG(1) << "WARNING: Could not find AppDb table: " << redis_key->table_name;
        return 0;
    }

    // Otherwise, we try to find the value. A status failure implies we could not
    // find the key so we can't delete it.
    auto status = app_db_table_iter->second->ReadTableEntry(redis_key->key);
    if (!status.ok())
    {
        VLOG(1) << "WARNING: Could not find AppDb entry: " << redis_key->key;
        return 0;
    }

    app_db_table_iter->second->DeleteTableEntry(redis_key->key);
    return 1;
}

void FakeDBConnector::del(const std::vector<std::string> &keys)
{
    for (const auto &key : keys)
    {
        FakeDBConnector::del(key);
    }
}

std::shared_ptr<std::string> FakeDBConnector::hget(const std::string &key, const std::string &field)
{
    VLOG(1) << "Getting value for field: " << field << " with key: " << key;

    auto field_value = hgetall(key);

    // Return the value if found.
    if (field_value.find(field) != field_value.end())
    {
        return std::make_shared<std::string>(field_value[field]);
    }
    return nullptr;
}

void FakeDBConnector::hmset(const std::string &key, const std::vector<FieldValueTuple> &values)
{
    // We should not be setting anything through this interface. The ownership
    // of entries should be handled by other interfaces
    // (e.g. ProducerStateTable, etc.)
    LOG(FATAL) << "Do not set AppDb entries through DBConnector.";
}

} // namespace swss
