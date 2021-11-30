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
    ProducerTable(DBConnector *db, const std::string &tableName);
    ProducerTable(RedisPipeline *pipeline, const std::string &tableName, bool buffered = false);
    ProducerTable(DBConnector *db, const std::string &tableName, const std::string &dumpFile);
    virtual ~ProducerTable();

    void setBuffered(bool buffered);

    /* Implements set() and del() commands using notification messages */

    virtual void set(const std::string &key,
                     const std::vector<FieldValueTuple> &values,
                     const std::string &op = SET_COMMAND,
                     const std::string &prefix = EMPTY_PREFIX);

    virtual void del(const std::string &key,
                     const std::string &op = DEL_COMMAND,
                     const std::string &prefix = EMPTY_PREFIX);

#ifdef SWIG
    // SWIG interface file (.i) globally rename map C++ `del` to python `delete`,
    // but applications already followed the old behavior of auto renamed `_del`.
    // So we implemented old behavior for backward compatibility
    // TODO: remove this function after applications use the function name `delete`
    %pythoncode %{
        def _del(self, *args, **kwargs):
            return self.delete(*args, **kwargs)
    %}
#endif

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

    void enqueueDbChange(const std::string &key, const std::string &value, const std::string &op, const std::string &prefix);
};

}

#endif
