#ifndef __INTFSYNC__
#define __INTFSYNC__

#include "common/dbconnector.h"
#include "common/producertable.h"
#include "common/netmsg.h"

namespace swss {

class IntfSync : public NetMsg
{
public:
    enum { MAX_ADDR_SIZE = 64 };

    IntfSync(DBConnector *db);

    virtual void onMsg(int nlmsg_type, struct nl_object *obj);

private:
    ProducerTable m_intfTable;
};

}

#endif
