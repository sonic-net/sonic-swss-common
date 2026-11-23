#include "zmqserver.h"
#include "../zmqserver.h"
#include "util.h"

using namespace swss;
using namespace std;

SWSSResult SWSSZmqServer_new(const char *endpoint, SWSSZmqServer *outZmqServer) {
    SWSSTry(*outZmqServer = (SWSSZmqServer) new ZmqServer(string(endpoint)));
}

SWSSResult SWSSZmqServer_new_unbound(const char *endpoint, SWSSZmqServer *outZmqServer) {
    SWSSTry(*outZmqServer = (SWSSZmqServer) new ZmqServer(string(endpoint), "", true));
}

SWSSResult SWSSZmqServer_bind(SWSSZmqServer zmqs) {
    SWSSTry(((ZmqServer *)zmqs)->bind());
}

SWSSResult SWSSZmqServer_free(SWSSZmqServer zmqs) {
    SWSSTry(delete (ZmqServer *)zmqs);
}
