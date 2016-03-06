#ifndef __NEIGHSYNC__
#define __NEIGHSYNC__

#include "common/dbconnector.h"
#include "common/producertable.h"
#include "common/netmsg.h"

namespace swss {

class NeighSync : public NetMsg
{
public:
    enum { MAX_ADDR_SIZE = 64 };

    NeighSync(DBConnector *db);

    virtual void onMsg(int nlmsg_type, struct nl_object *obj);

private:
    ProducerTable m_neighTable;
};

}

#endif
