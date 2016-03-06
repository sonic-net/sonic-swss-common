#ifndef __CONSUMERTABLE__
#define __CONSUMERTABLE__

#include <string>
#include <vector>
#include <limits>
#include <hiredis/hiredis.h>
#include "common/dbconnector.h"
#include "common/table.h"
#include "common/selectable.h"

namespace swss {

class ConsumerTable : public Table, public Selectable
{
public:
    ConsumerTable(DBConnector *db, std::string tableName);
    virtual ~ConsumerTable();

    /* Get a singlesubsribe channel rpop */
    void pop(KeyOpFieldsValuesTuple &kco);

    /* Get a key content without poping it from the notificaiton list */
    /* return false if the key doesn't exists */
    bool peek(std::string key, std::vector<FieldValueTuple> &values);

    virtual void addFd(fd_set *fd);
    virtual bool isMe(fd_set *fd);
    virtual int readCache();
    virtual void readMe();

private:
    /* Create a new redisContext, SELECT DB and SUBSRIBE */
    void subsribe();

    DBConnector *m_subscribe;
    unsigned int m_queueLength;
};

}

#endif
