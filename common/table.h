#ifndef __TABLE__
#define __TABLE__

#include <assert.h>
#include <string>
#include <queue>
#include <tuple>
#include <map>
#include <deque>
#include "hiredis/hiredis.h"
#include "dbconnector.h"
#include "redisreply.h"
#include "redisselect.h"
#include "schema.h"
#include "redistran.h"

namespace swss {

#define DEFAULT_TABLE_NAME_SEPARATOR    ":"
#define CONFIGDB_TABLE_NAME_SEPARATOR   "|"

typedef std::tuple<std::string, std::string> FieldValueTuple;
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
    TableBase(std::string tableName, std::string tableSeparator = DEFAULT_TABLE_NAME_SEPARATOR)
        : m_tableName(tableName), m_tableSeparator(tableSeparator)
    {
        const std::string legalSeparators = ":|";
        if (legalSeparators.find(tableSeparator) == std::string::npos)
            throw std::invalid_argument("Invalid table name separator");
    }

    std::string getTableName() const { return m_tableName; }

    /* Return the actual key name as a comibation of tableName:key */
    std::string getKeyName(std::string key)
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
    std::string m_tableName;
    std::string m_tableSeparator;
};

class TableEntryWritable {
public:
    virtual ~TableEntryWritable() { }

    /* Set an entry in the table */
    virtual void set(std::string key,
                     std::vector<FieldValueTuple> &values,
                     std::string op = "",
                     std::string prefix = EMPTY_PREFIX) = 0;
    /* Delete an entry in the table */
    virtual void del(std::string key,
                     std::string op = "",
                     std::string prefix = EMPTY_PREFIX) = 0;

};

class TableEntryPoppable {
public:
    virtual ~TableEntryPoppable() { }

    /* Pop an action (set or del) on the table */
    virtual void pop(KeyOpFieldsValuesTuple &kco, std::string prefix = EMPTY_PREFIX) = 0;

    /* Get multiple pop elements */
    virtual void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string prefix = EMPTY_PREFIX) = 0;
};

class TableConsumable : public TableBase, public TableEntryPoppable, public RedisSelect {
public:
    /* The default value of pop batch size is 128 */
    static constexpr int DEFAULT_POP_BATCH_SIZE = 128;

    TableConsumable(std::string tableName) : TableBase(tableName) { }
};

class TableEntryEnumerable {
public:
    virtual ~TableEntryEnumerable() { }

    /* Get all the field-value tuple of the table entry with the key */
    virtual bool get(std::string key, std::vector<FieldValueTuple> &values) = 0;

    /* get all the keys in the table */
    virtual void getTableKeys(std::vector<std::string> &keys) = 0;

    /* Read the whole table content from the DB directly */
    /* NOTE: Not an atomic function */
    void getTableContent(std::vector<KeyOpFieldsValuesTuple> &tuples);
};

class Table : public RedisTransactioner, public TableBase, public TableEntryEnumerable {
public:
    Table(DBConnector *db, std::string tableName, std::string tableSeparator = DEFAULT_TABLE_NAME_SEPARATOR);
    virtual ~Table() { }

    /* Set an entry in the DB directly (op not in use) */
    virtual void set(std::string key,
                     std::vector<FieldValueTuple> &values,
                     std::string op = "",
                     std::string prefix = EMPTY_PREFIX);
    /* Delete an entry in the table */
    virtual void del(std::string key,
                     std::string op = "",
                     std::string prefix = EMPTY_PREFIX);

    /* Read a value from the DB directly */
    /* Returns false if the key doesn't exists */
    virtual bool get(std::string key, std::vector<FieldValueTuple> &values);

    void getTableKeys(std::vector<std::string> &keys);

    void dump(TableDump &tableDump);
};

class TableName_KeyValueOpQueues {
private:
    std::string m_key;
    std::string m_value;
    std::string m_op;
public:
    TableName_KeyValueOpQueues(std::string tableName)
        : m_key(tableName + "_KEY_QUEUE")
        , m_value(tableName + "_VALUE_QUEUE")
        , m_op(tableName + "_OP_QUEUE")
    {
    }

    std::string getKeyQueueTableName() const { return m_key; }
    std::string getValueQueueTableName() const { return m_value; }
    std::string getOpQueueTableName() const { return m_op; }
};

class TableName_KeySet {
private:
    std::string m_key;
public:
    TableName_KeySet(std::string tableName)
        : m_key(tableName + "_KEY_SET")
    {
    }

    std::string getKeySetName() const { return m_key; }
};

}
#endif
