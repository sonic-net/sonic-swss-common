#ifndef __PRODUCERTABLE__
#define __PRODUCERTABLE__

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "table.h"
#include "redisselect.h"
#include "redispipeline.h"

namespace swss {

class ProducerTable : public TableBase, public TableName_KeyValueOpQueues
{
public:
    ProducerTable(DBConnector *db, std::string tableName);
    ProducerTable(RedisPipeline *pipeline, std::string tableName, bool buffered = false);
    ProducerTable(DBConnector *db, std::string tableName, std::string dumpFile);
    virtual ~ProducerTable();

    void setBuffered(bool buffered);

    /* Implements set() and del() commands using notification messages */

    virtual void set(std::string key,
                     std::vector<FieldValueTuple> &values,
                     std::string op = SET_COMMAND,
                     std::string prefix = EMPTY_PREFIX);

    virtual void del(std::string key,
                     std::string op = DEL_COMMAND,
                     std::string prefix = EMPTY_PREFIX);

    void flush();

private:
    /* Disable copy-constructor and operator = */
    ProducerTable(const ProducerTable &other);
    ProducerTable & operator = (const ProducerTable &other);

    std::ofstream m_dumpFile;
    bool m_firstItem = true;
    bool m_buffered;
    bool m_pipeowned;
    RedisPipeline *m_pipe;
    std::string m_shaEnque;

    void enqueueDbChange(std::string key, std::string value, std::string op, std::string prefix);
};

}

#endif
