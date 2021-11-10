#pragma once

#include <memory>
#include <vector>

#include "consumernotifierinterface.h"
#include "dbconnector.h"
#include "notificationconsumer.h"

namespace swss
{

// Wrapper class to use swss::NotificationConsumer.
class ConsumerNotifier : public ConsumerNotifierInterface
{
  public:
    ConsumerNotifier(const std::string &notifier_channel_name, DBConnector *db_connector);

    bool WaitForNotificationAndPop(std::string &op, std::string &data, std::vector<FieldValueTuple> &values,
                                   int64_t timeout_ms = 60000LL) override;

  private:
    std::unique_ptr<NotificationConsumer> notification_consumer_;
    std::string notifier_channel_name_;
};

} // namespace swss
