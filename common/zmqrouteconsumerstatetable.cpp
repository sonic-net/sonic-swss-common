#include "dbconnector.h"
#include "selectable.h"
#include "zmqrouteconsumerstatetable.h"
#include "logger.h"

using namespace std;

namespace swss {

void ZmqRouteConsumerStateTable::handleReceivedData(const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos)
{
    // Hand tuples straight to the consumer (mqPollThread). The SelectableEvent
    // is *not* fired here; mqPollThread fires it via notifyPending() after the
    // socket drains, and the callback may also fire it mid-burst when
    // m_toSync grows past gMaxBulkSize.
    if (m_ingressCallback)
    {
        m_ingressCallback(kcos);
    }

    if (m_asyncDBUpdater != nullptr)
    {
        for (const auto& kco : kcos)
        {
            m_asyncDBUpdater->update(std::make_shared<KeyOpFieldsValuesTuple>(*kco));
        }
    }
}

}
