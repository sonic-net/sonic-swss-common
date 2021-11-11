#pragma once

#include <vector>

#include "absl/status/status.h"
#include "swss/consumernotifierinterface.h"
#include "swss/fakes/fake_sonic_db_table.h"

namespace swss
{

// Fakes the OrchAgent response path behavior for AppDb table entries.
//
// Every write into an AppDb table is handled by the OrchAgent. The write can
// either succeed or fail. In the latter case a failed StatusCode should be
// returned by the fake.
class FakeConsumerNotifier final : public ConsumerNotifierInterface
{
  public:
    explicit FakeConsumerNotifier(FakeSonicDbTable *app_db_table);

    // Not copyable or moveable.
    FakeConsumerNotifier(const FakeConsumerNotifier &) = delete;
    FakeConsumerNotifier &operator=(const FakeConsumerNotifier &) = delete;

    // Faked methods.
    bool WaitForNotificationAndPop(std::string &op, std::string &data, SonicDbEntry &values,
                                   int64_t timeout_ms = 60000LL) override;

  private:
    // The AppDb table maintains a list of notifications that this fake can
    // request.
    FakeSonicDbTable *app_db_table_; // No ownership.
};

} // namespace swss
