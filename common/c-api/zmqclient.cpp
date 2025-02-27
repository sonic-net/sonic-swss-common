#include "../zmqclient.h"
#include "../binaryserializer.h"
#include "util.h"
#include "zmqclient.h"

using namespace swss;
using namespace std;

SWSSResult SWSSZmqClient_new(const char *endpoint, SWSSZmqClient *outClient) {
    SWSSTry(*outClient = (SWSSZmqClient) new ZmqClient(endpoint));
}

SWSSResult SWSSZmqClient_free(SWSSZmqClient zmqc) {
    SWSSTry(delete (ZmqClient *)zmqc);
}

SWSSResult SWSSZmqClient_isConnected(SWSSZmqClient zmqc, int8_t *outIsConnected) {
    SWSSTry(*outIsConnected = ((ZmqClient *)zmqc)->isConnected() ? 1 : 0);
}

SWSSResult SWSSZmqClient_connect(SWSSZmqClient zmqc) {
    SWSSTry(((ZmqClient *)zmqc)->connect());
}

SWSSResult SWSSZmqClient_sendMsg(SWSSZmqClient zmqc, const char *dbName, const char *tableName,
                                 SWSSKeyOpFieldValuesArray arr) {
    SWSSTry({
        vector<KeyOpFieldsValuesTuple> kcos = takeKeyOpFieldValuesArray(arr);
        ((ZmqClient *)zmqc)
            ->sendMsg(string(dbName), string(tableName), kcos);
    });
}
