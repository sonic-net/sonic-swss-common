#pragma once

#include <queue>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"

namespace swss
{

// When interacting with the Redis database through a SONiC interface we
// typically use the swss:FieldValueTuple (i.e. pair<string, string>). To keep
// the Fakes independent we redefine the alias.
using SonicDbKeyValue = std::pair<std::string, std::string>;

// The Redis database entries act like a list of key value pairs.
using SonicDbEntry = std::vector<SonicDbKeyValue>;

// Fakes how the OrchAgent updates AppDb tables. When an entry is inserted the
// Orchagent will respond with a notification of success or failure.
//
// This class is NOT thread-safe.
class FakeSonicDbTable
{
  public:
    void InsertTableEntry(const std::string &key, const SonicDbEntry &values);
    void DeleteTableEntry(const std::string &key);

    void SetResponseForKey(const std::string &key, const std::string &code, const std::string &message);
    void GetNextNotification(std::string &op, std::string &data, SonicDbEntry &values);

    absl::StatusOr<SonicDbEntry> ReadTableEntry(const std::string &key) const;

    std::vector<std::string> GetAllKeys() const;

    // Method should only be used for debug purposes.
    void DebugState() const;

  private:
    struct ResponseInfo
    {
        std::string code;
        std::string message;
    };

    // Current list of DB entries stored in the table.
    absl::flat_hash_map<std::string, SonicDbEntry> entries_;

    // List of notifications the OrchAgent would have generated. One notification
    // is created per insert, and one is removed per notification check.
    std::queue<std::string> notifications_;

    // By default all notifications will return success. To fake an error case we
    // need to set the expected response for an AppDb key.
    absl::flat_hash_map<std::string, ResponseInfo> responses_;
};

} // namespace swss
