#pragma once

#include <memory>
#include "table.h"
#include "redispipeline.h"

namespace swss {

class ProducerStateTable : public TableBase, public TableName_KeySet
{
public:
    ProducerStateTable(DBConnector *db, const std::string &tableName);
    ProducerStateTable(RedisPipeline *pipeline, const std::string &tableName, bool buffered = false);
    ~ProducerStateTable();

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

    int64_t count();

    void clear();

    void create_temp_view();

    void apply_temp_view();
private:
    bool m_buffered;
    bool m_pipeowned;
    bool m_tempViewActive;
    RedisPipeline *m_pipe;
    std::string m_shaSet;
    std::string m_shaDel;
    std::string m_shaClear;
    std::string m_shaApplyView;
    TableDump m_tempViewState;
};

}
