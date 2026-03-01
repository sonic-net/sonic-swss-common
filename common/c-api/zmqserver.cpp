#include "zmqserver.h"
#include "../zmqserver.h"
#include "util.h"

using namespace swss;
using namespace std;

SWSSResult SWSSZmqServer_new(const char *endpoint, SWSSZmqServer *outZmqServer) {
  SWSSTry(*outZmqServer = (SWSSZmqServer) new ZmqServer(string(endpoint), std::string("")));
}

SWSSResult SWSSZmqServer_free(SWSSZmqServer zmqs) {
    SWSSTry(delete (ZmqServer *)zmqs);
}
