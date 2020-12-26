#include "configdb.h"
#include "pubsub.h"

using namespace std;
using namespace swss;

ConfigDBConnector::ConfigDBConnector(bool use_unix_socket_path, const char *netns)
    : SonicV2Connector(use_unix_socket_path, netns)
{
}

void ConfigDBConnector::connect(bool wait_for_init, bool retry_on)
{
    m_db_name = "CONFIG_DB";
    SonicV2Connector::connect(m_db_name, retry_on);

    if (wait_for_init)
    {
        auto client = get_redis_client(m_db_name);
        auto pubsub = client.pubsub();
        auto initialized = client.get(INIT_INDICATOR);
        if (initialized)
        {
            string pattern = "__keyspace@" + to_string(get_dbid(m_db_name)) +  "__:" + INIT_INDICATOR;
            pubsub->psubscribe(pattern);
            for (;;)
            {
                auto item = pubsub->listen_message();
                if (item["type"] == "pmessage")
                {
                    string channel = item["channel"];
                    size_t pos = channel.find(':');
                    string key = channel.substr(pos + 1);
                    if (key == INIT_INDICATOR)
                    {
                        initialized = client.get(INIT_INDICATOR);
                        if (initialized && !initialized->empty())
                        {
                            break;
                        }
                    }
                }
            }
            pubsub->punsubscribe(pattern);
        }
    }
}

