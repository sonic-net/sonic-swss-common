#pragma once

#include <vector>

#include "table.h"

namespace swss
{

// Wrapper class to use swss::NotificationConsumer.
class ConsumerNotifierInterface
{
  public:
    virtual ~ConsumerNotifierInterface() = default;

    // Waits for notifications to be posted on the response channel,
    // this goes into a blocking wait (default timeout of 60 seconds) until a
    // response is received and then uses swss::pop to get the response values.
    virtual bool WaitForNotificationAndPop(std::string &op, std::string &data, std::vector<FieldValueTuple> &values,
                                           int64_t timeout_ms = 60000LL) = 0;
};

} // namespace swss
