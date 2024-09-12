#include "zmqserver.h"
#include "../zmqserver.h"
#include "util.h"

using namespace swss;
using namespace std;

class FunctionPointerMessageHandler : public ZmqMessageHandler {
  public:
    FunctionPointerMessageHandler(void *data, SWSSZmqMessageHandlerCallback callback)
        : m_data(data), m_callback(callback) {}

    virtual void
    handleReceivedData(const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos) override {
        SWSSKeyOpFieldValuesArray arr = makeKeyOpFieldValuesArray(kcos);
        m_callback(m_data, &arr);
    }

    void *m_data;
    SWSSZmqMessageHandlerCallback m_callback;
};

SWSSZmqMessageHandler SWSSZmqMessageHandler_new(void *data,
                                                SWSSZmqMessageHandlerCallback callback) {
    SWSSTry(return (SWSSZmqMessageHandler) new FunctionPointerMessageHandler(data, callback));
}

void SWSSZmqMessageHandler_free(SWSSZmqMessageHandler handler) {
    SWSSTry(delete (FunctionPointerMessageHandler *)handler);
}

SWSSZmqServer SWSSZmqServer_new(const char *endpoint) {
    SWSSTry(return (SWSSZmqServer) new ZmqServer(string(endpoint)));
}

void SWSSZmqServer_free(SWSSZmqServer zmqs) {
    SWSSTry(delete (ZmqServer *)zmqs);
}

void SWSSZmqServer_registerMessageHandler(SWSSZmqServer zmqs, const char *dbName,
                                          const char *tableName, SWSSZmqMessageHandler handler) {
    SWSSTry(((ZmqServer *)zmqs)
                ->registerMessageHandler(string(dbName), string(tableName),
                                         (FunctionPointerMessageHandler *)handler));
}
