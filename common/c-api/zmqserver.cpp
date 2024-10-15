#include "zmqserver.h"
#include "../zmqserver.h"
#include "util.h"

using namespace swss;
using namespace std;

SWSSZmqServer SWSSZmqServer_new(const char *endpoint) {
    SWSSTry(return (SWSSZmqServer) new ZmqServer(string(endpoint)));
}

void SWSSZmqServer_free(SWSSZmqServer zmqs) {
    SWSSTry(delete (ZmqServer *)zmqs);
}
