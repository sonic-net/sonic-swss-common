#include "../zmqclient.h"
#include "../binaryserializer.h"
#include "util.h"
#include "zmqclient.h"

using namespace swss;
using namespace std;

SWSSZmqClient SWSSZmqClient_new(const char *endpoint) {
    SWSSTry(return (SWSSZmqClient) new ZmqClient(endpoint));
}

void SWSSZmqClient_free(SWSSZmqClient zmqc) {
    SWSSTry(delete (ZmqClient *)zmqc);
}

// Returns 0 for false, 1 for true
int8_t SWSSZmqClient_isConnected(SWSSZmqClient zmqc) {
    SWSSTry(return ((ZmqClient *)zmqc)->isConnected() ? 1 : 0);
}

void SWSSZmqClient_connect(SWSSZmqClient zmqc) {
    SWSSTry(((ZmqClient *)zmqc)->connect());
}

void SWSSZmqClient_sendMsg(SWSSZmqClient zmqc, const char *dbName, const char *tableName,
                           SWSSKeyOpFieldValuesArray arr) {
    SWSSTry({
        vector<KeyOpFieldsValuesTuple> kcos = takeKeyOpFieldValuesArray(arr);
        size_t bufSize = BinarySerializer::serializedSize(dbName, tableName, kcos);
        vector<char> v(bufSize);
        ((ZmqClient *)zmqc)
            ->sendMsg(string(dbName), string(tableName), kcos, v);
    });
}
