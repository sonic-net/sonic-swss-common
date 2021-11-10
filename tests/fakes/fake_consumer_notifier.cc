#include "swss/fakes/fake_consumer_notifier.h"

#include "glog/logging.h"

namespace swss
{

FakeConsumerNotifier::FakeConsumerNotifier(FakeSonicDbTable *app_db_table) : app_db_table_(app_db_table)
{
    LOG_IF(FATAL, app_db_table == nullptr) << "FakeSonicDbTable cannot be nullptr.";
}

bool FakeConsumerNotifier::WaitForNotificationAndPop(std::string &op, std::string &data, SonicDbEntry &values,
                                                     int64_t timeout_ms)
{
    app_db_table_->GetNextNotification(op, data, values);
    return true;
}

} // namespace swss
