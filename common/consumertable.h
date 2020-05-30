#ifndef __CONSUMERTABLE__
#define __CONSUMERTABLE__

#include <string>
#include <deque>
#include <limits>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "consumerstatetable.h"

namespace swss {

class ConsumerTable : public ConsumerTableBase, public TableName_KeyValueOpQueues
{
public:
    ConsumerTable(DBConnector *db, const std::string &tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE, int pri = 0);

    /* Get multiple pop elements */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string &prefix = EMPTY_PREFIX);

    void setModifyRedis(bool modify);
private:
    std::string m_shaPop;

    /**
     * @brief Modify Redis database.
     *
     * If set to false, will not make changes to database during POPs operation.
     * This will be utilized during synchronous mode.
     *
     * Default is true.
     */
    bool m_modifyRedis;
};

}

#endif
