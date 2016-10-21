#ifndef __TABLE__
#define __TABLE__

#include <assert.h>
#include <string>
#include <queue>
#include <tuple>
#include "hiredis/hiredis.h"
#include "dbconnector.h"
#include "redisreply.h"
#include "schema.h"

namespace swss {

typedef std::tuple<std::string, std::string> FieldValueTuple;
#define fvField std::get<0>
#define fvValue std::get<1>
typedef std::tuple<std::string, std::string, std::vector<FieldValueTuple> > KeyOpFieldsValuesTuple;
#define kfvKey    std::get<0>
#define kfvOp     std::get<1>
#define kfvFieldsValues std::get<2>

class TableName {
public:
    TableName(std::string tableName) : m_tableName(tableName) { }

    std::string getTableName() const { return m_tableName; }
    
    /* Return the actual key name as a comibation of tableName:key */
    std::string getKeyName(std::string key) { assert(!key.empty()); return m_tableName + ':' + key; }
    
private:
    std::string m_tableName;
};

class TableEntryWritable {
public:
    /* Set an entry in the table */
    virtual void set(std::string key, std::vector<FieldValueTuple> &values,
                     std::string op = "") = 0;
    /* Delete an entry in the table */
    virtual void del(std::string key, std::string op = "") = 0;
};

class TableEntryReadable {
public:
    /* Get all the field-value tuple of the table entry with the key */
    virtual bool get(std::string key, std::vector<FieldValueTuple> &values) = 0;
};

class TableEntryEnumerable : public TableEntryReadable {
public:
    /* get all the keys in the table */
    virtual void getTableKeys(std::vector<std::string> &keys) = 0;
    
    /* Read the whole table content from the DB directly */
    /* NOTE: Not an atomic function */
    void getTableContent(std::vector<KeyOpFieldsValuesTuple> &tuples);
};

class RedisFormatter {
public:
    /* Format HMSET key multiple field value command */
    static std::string formatHMSET(const std::string &key,
                                   const std::vector<FieldValueTuple> &values);

    /* Format HSET key field value command */
    static std::string formatHSET(const std::string &key,
                                  const std::string &field,
                                  const std::string &value);

    /* Format HGET key field command */
    static std::string formatHGET(const std::string& key,
                                  const std::string& field);

    /* Format HDEL key field command */
    static std::string formatHDEL(const std::string& key,
                                  const std::string& field);
};

class RedisTransactioner {
public:
    RedisTransactioner(DBConnector *db) : m_db(db) { }
    
    /* Start a transaction */
    void multi();
    /* Execute a transaction and get results */
    void exec();
    /* Send a command within a transaction */
    void enqueue(std::string command, int exepectedResult, bool isFormatted = false);
    
    redisReply* queueResultsFront();
    std::string queueResultsPop();

    std::string scriptLoad(const std::string& script);

protected:
    DBConnector *m_db;
    
private:    
    /* Remember the expected results for the transaction */
    std::queue<int> m_expectedResults;
    std::queue<RedisReply *> m_results;
};

class Table : public TableName, public RedisTransactioner, public RedisFormatter, public TableEntryEnumerable {
public:
    Table(DBConnector *db, std::string tableName);

    /* Set an entry in the DB directly (op not in used) */
    virtual void set(std::string key, std::vector<FieldValueTuple> &values,
                     std::string op = "");
    /* Delete an entry in the DB directly (op not in used) */
    virtual void del(std::string key, std::string op = "");
    
    /* Read a value from the DB directly */
    /* Returns false if the key doesn't exists */
    virtual bool get(std::string key, std::vector<FieldValueTuple> &values);
    
    void getTableKeys(std::vector<std::string> &keys);
};

class TableName_KeyValueOpQueues : public TableName {
public:
    TableName_KeyValueOpQueues(std::string tableName) : TableName(tableName) { }
    std::string getKeyQueueTableName() { return getTableName() + "_KEY_QUEUE"; }
    std::string getValueQueueTableName() { return getTableName() + "_VALUE_QUEUE"; }
    std::string getOpQueueTableName() { return getTableName() + "_OP_QUEUE"; }
    std::string getChannelTableName() { return getTableName() + "_CHANNEL"; }
};

}
#endif
