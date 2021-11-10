#pragma once

#include <vector>

#include "swss/fakes/fake_sonic_db_table.h"
#include "swss/producerstatetableinterface.h"

namespace swss
{

// Fake the OrchAgent write path behavior for AppDb table entries.
class FakeProducerStateTable final : public ProducerStateTableInterface
{
  public:
    FakeProducerStateTable(const std::string &table_name, FakeSonicDbTable *app_db_table);

    // Not copyable or moveable.
    FakeProducerStateTable(const FakeProducerStateTable &) = delete;
    FakeProducerStateTable &operator=(const FakeProducerStateTable &) = delete;

    // Faked methods.
    void set(const std::string &key, const std::vector<FieldValueTuple> &values, const std::string &op = SET_COMMAND,
             const std::string &prefix = EMPTY_PREFIX) override;
    void set(const std::vector<KeyOpFieldsValuesTuple> &values);
    void del(const std::string &key, const std::string &op = DEL_COMMAND,
             const std::string &prefix = EMPTY_PREFIX) override;
    void del(const std::vector<std::string> &keys);

    std::string get_table_name() const override
    {
        return table_name_;
    }

  private:
    const std::string table_name_;

    // The AppDb table maintains a list of all installed table entries created by
    // this fake.
    FakeSonicDbTable *app_db_table_; // No ownership.
};

} // namespace swss
