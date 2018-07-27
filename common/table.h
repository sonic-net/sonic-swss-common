#ifndef __TABLE__
#define __TABLE__

#include <assert.h>
#include <string>
#include <queue>
#include <tuple>
#include <utility>
#include <map>
#include <deque>
#include "hiredis/hiredis.h"
#include "dbconnector.h"
#include "redisreply.h"
#include "redisselect.h"
#include "redispipeline.h"
#include "schema.h"
#include "redistran.h"

namespace swss {

// Mapping of DB ID to table name separator string
typedef std::map<int, std::string> TableNameSeparatorMap;

typedef std::pair<std::string, std::string> FieldValueTuple;
#define fvField std::get<0>
#define fvValue std::get<1>
typedef std::tuple<std::string, std::string, std::vector<FieldValueTuple> > KeyOpFieldsValuesTuple;
#define kfvKey    std::get<0>
#define kfvOp     std::get<1>
#define kfvFieldsValues std::get<2>

typedef std::map<std::string,std::string> TableMap;
typedef std::map<std::string,TableMap> TableDump;

class TableBase {
public:
    TableBase(int dbId, const std::string &tableName)
        : m_tableName(tableName), m_dbId(dbId)
    {
        /* Look up table separator for the provided DB */
        auto it = tableNameSeparatorMap.find(dbId);

        if (it != tableNameSeparatorMap.end())
        {
            m_tableSeparator = it->second;
        }
        else
        {
            SWSS_LOG_NOTICE("Unrecognized database ID. Using default table name separator ('%s')", TABLE_NAME_SEPARATOR_VBAR.c_str());
            m_tableSeparator = TABLE_NAME_SEPARATOR_VBAR;
        }
    }

    std::string getTableName() const { return m_tableName; }
    int getDbId() const { return m_dbId; }

    /* Return the actual key name as a combination of tableName<table_separator>key */
    std::string getKeyName(const std::string &key)
    {
        if (key == "") return m_tableName;
        else return m_tableName + m_tableSeparator + key;
    }

    /* Return the table name separator being used */
    std::string getTableNameSeparator() const
    {
        return m_tableSeparator;
    }

    std::string getChannelName() { return m_tableName + "_CHANNEL"; }
private:
    static const std::string TABLE_NAME_SEPARATOR_COLON;
    static const std::string TABLE_NAME_SEPARATOR_VBAR;
    static const TableNameSeparatorMap tableNameSeparatorMap;

    std::string m_tableName;
    std::string m_tableSeparator;
    int m_dbId;
};

class TableEntryWritable {
public:
    virtual ~TableEntryWritable() = default;

    /* Set an entry in the table */
    virtual void set(const std::string &key,
                     const std::vector<FieldValueTuple> &values,
                     const std::string &op = "",
                     const std::string &prefix = EMPTY_PREFIX) = 0;
    /* Delete an entry in the table */
    virtual void del(const std::string &key,
                     const std::string &op = "",
                     const std::string &prefix = EMPTY_PREFIX) = 0;

};

class TableEntryPoppable {
public:
    virtual ~TableEntryPoppable() = default;

    /* Pop an action (set or del) on the table */
    virtual void pop(KeyOpFieldsValuesTuple &kco, const std::string &prefix = EMPTY_PREFIX) = 0;

    /* Get multiple pop elements */
    virtual void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string &prefix = EMPTY_PREFIX) = 0;
};

class TableConsumable : public TableBase, public TableEntryPoppable, public RedisSelect {
public:
    /* The default value of pop batch size is 128 */
    static constexpr int DEFAULT_POP_BATCH_SIZE = 128;

    TableConsumable(int dbId, const std::string &tableName, int pri) : TableBase(dbId, tableName), RedisSelect(pri) { }
};

class TableEntryEnumerable {
public:
    virtual ~TableEntryEnumerable() = default;

    /* Get all the field-value tuple of the table entry with the key */
    virtual bool get(const std::string &key, std::vector<FieldValueTuple> &values) = 0;

    /* get all the keys in the table */
    virtual void getKeys(std::vector<std::string> &keys) = 0;

    /* Read the whole table content from the DB directly */
    /* NOTE: Not an atomic function */
    void getContent(std::vector<KeyOpFieldsValuesTuple> &tuples);
};

class Table : public TableBase, public TableEntryEnumerable {
public:
    Table(DBConnector *db, const std::string &tableName);
    Table(RedisPipeline *pipeline, const std::string &tableName, bool buffered);
    ~Table() override;

    /* Set an entry in the DB directly (op not in use) */
    virtual void set(const std::string &key,
                     const std::vector<FieldValueTuple> &values,
                     const std::string &op = "",
                     const std::string &prefix = EMPTY_PREFIX);
    /* Delete an entry in the table */
    virtual void del(const std::string &key,
                     const std::string &op = "",
                     const std::string &prefix = EMPTY_PREFIX);

    /* Read a value from the DB directly */
    /* Returns false if the key doesn't exists */
    virtual bool get(const std::string &key, std::vector<FieldValueTuple> &ovalues);

    void getKeys(std::vector<std::string> &keys);

    void setBuffered(bool buffered);

    void flush();

    void dump(TableDump &tableDump);

protected:

    bool m_buffered;
    bool m_pipeowned;
    RedisPipeline *m_pipe;

    /* Strip special symbols from keys used for type identification
     * Input example:
     *     port@
     * DB entry:
     * 1) "ports@"
     * 2) "Ethernet0,Ethernet4,...
     * */
    std::string stripSpecialSym(const std::string &key);
};

class TableName_KeyValueOpQueues {
private:
    std::string m_keyvalueop;
public:
    TableName_KeyValueOpQueues(const std::string &tableName)
        : m_keyvalueop(tableName + "_KEY_VALUE_OP_QUEUE")
    {
    }

    std::string getKeyValueOpQueueTableName() const { return m_keyvalueop; }
};

class TableName_KeySet {
private:
    std::string m_key;
public:
    TableName_KeySet(const std::string &tableName)
        : m_key(tableName + "_KEY_SET")
    {
    }

    std::string getKeySetName() const { return m_key; }
    std::string getStateHashPrefix() const { return "_"; }
};

}
#endif
