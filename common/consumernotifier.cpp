#include "consumernotifier.h"

#include "select.h"
#include "selectable.h"

namespace swss
{

ConsumerNotifier::ConsumerNotifier(const std::string &notifier_channel_name, DBConnector *db_connector)
{
    notifier_channel_name_ = std::string{notifier_channel_name};
    notification_consumer_ = std::make_unique<NotificationConsumer>(db_connector, notifier_channel_name_);
}

bool ConsumerNotifier::WaitForNotificationAndPop(std::string &op, std::string &data,
                                                 std::vector<FieldValueTuple> &values, int64_t timeout_ms)
{
    int message_available = notification_consumer_->peek();
    if (message_available == -1)
    {
        return false;
    }

    // Wait for notification only when the queue is empty.
    if (message_available == 0)
    {
        Select s;
        s.addSelectable(notification_consumer_.get());
        Selectable *sel;

        int error = s.select(&sel, timeout_ms);
        if (error != Select::OBJECT)
        {
            return false;
        }
    }
    notification_consumer_->pop(op, data, values);
    return true;
}

} // namespace swss
