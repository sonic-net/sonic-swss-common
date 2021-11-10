#pragma once

#include <map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "swss/dbconnectorinterface.h"
#include "swss/fakes/fake_sonic_db_table.h"
#include "swss/table.h"

namespace swss
{

// Fake the behavior of SONiC's DBConnector class.
//
// The DBConnector is able to read and modify any table (e.g. P4RT or VRF_TABLE)
// in a redis DB (e.g. AppDb, ConfigDb, AsicDb, etc.).
class FakeDBConnector final : public DBConnectorInterface
{
  public:
    explicit FakeDBConnector(const std::string &delimiter) : delimiter_(delimiter)
    {
    }

    // Not copyable or moveable.
    FakeDBConnector(const FakeDBConnector &) = delete;
    FakeDBConnector &operator=(const FakeDBConnector &) = delete;

    // Allows access to a fake SONiC Redis DB table.
    void AddAppDbTable(const std::string &table_name, FakeSonicDbTable *table);

    // Faked methods.
    std::vector<std::string> keys(const std::string &glob) override;
    std::unordered_map<std::string, std::string> hgetall(const std::string &key) override;
    bool exists(const std::string &key) override;
    int64_t del(const std::string &key) override;
    void del(const std::vector<std::string> &keys) override;
    std::shared_ptr<std::string> hget(const std::string &key, const std::string &field) override;

    // The hmset() method is faked to keep parity with the adapter class. However,
    // it should not be used directly by the P4RT App. There are usually better
    // interface alternatives to handle that behavior.
    void hmset(const std::string &key, const std::vector<FieldValueTuple> &values) override;

  private:
    // The DBConnector interface has access to all tables (e.g. P4RT, or
    // VRF_TABLE) in a redis DB (e.g. AppDb). To fake this behavior we maintain a
    // map of any faked tables the client should have access to.
    //
    // key: name of the SONiC DB table (i.e. P4RT)
    // val: faked table holding all installed entries.
    absl::flat_hash_map<std::string, FakeSonicDbTable *> sonic_db_tables_; // No ownership.
    std::string delimiter_;
};

} // namespace swss
