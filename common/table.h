#ifndef __TABLE__
#define __TABLE__

#include <assert.h>
#include <string>
#include <queue>
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

class TableBase {
public:
    TableBase(std::string tableName) : m_tableName(tableName) { }

    std::string getTableName() const { return m_tableName; }
    
    /* Return the actual key name as a comibation of tableName:key */
    std::string getKeyName(std::string key) { assert(!key.empty()); return m_tableName + ':' + key; }
    
    std::string getChannelName() { return m_tableName + "_CHANNEL"; }
private:
    std::string m_tableName;
};

class TableEntryWritable {
public:
    virtual ~TableEntryWritable() { }
    
    /* Set an entry in the table */
    virtual void set(std::string key, std::vector<FieldValueTuple> &values,
                     std::string op = "") = 0;
    /* Delete an entry in the table */
    virtual void del(std::string key, std::string op = "") = 0;
    
};

class TableEntryPopable {
public:
    virtual ~TableEntryPopable() { }
    
    /* Pop an written action (set or del) on the table */
    virtual void pop(KeyOpFieldsValuesTuple &kco) = 0;
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

class RedisTransactioner {
public:
    RedisTransactioner(DBConnector *db) : m_db(db) { }
    
    /* Start a transaction */
    void multi();
    /* Execute a transaction and get results */
    bool exec();
    /* Send a command within a transaction */
    void enqueue(std::string command, int exepectedResult);
    
    redisReply* queueResultsFront();
    std::string queueResultsPop();
    
protected:
    DBConnector *m_db;
private:    
    /* Remember the expected results for the transaction */
    std::queue<int> m_expectedResults;
    std::queue<RedisReply *> m_results;
};

class Table : public RedisTransactioner, public TableBase, public TableEntryEnumerable {
public:
    Table(DBConnector *db, std::string tableName);
    virtual ~Table() { }

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

class TableName_KeyValueOpQueues : public TableBase {
public:
    TableName_KeyValueOpQueues(std::string tableName) : TableBase(tableName) { }
    std::string getKeyQueueTableName() { return getTableName() + "_KEY_QUEUE"; }
    std::string getValueQueueTableName() { return getTableName() + "_VALUE_QUEUE"; }
    std::string getOpQueueTableName() { return getTableName() + "_OP_QUEUE"; }
};

class TableName_KeySet : public TableBase {
public:
    TableName_KeySet(std::string tableName) : TableBase(tableName) { }
    std::string getKeySetName() { return getTableName() + "_KEY_SET"; }
};

}
#endif
