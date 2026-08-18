#pragma once

#include <map>
#include <memory>
#include <thread>
#include "zmqserver.h"

namespace swss {

class ZmqRouteServer : public ZmqServer
{
  public:
  ZmqRouteServer(const std::string& endpoint,
                 const std::string& vrf,
                 bool lazyBind)
    : ZmqServer(endpoint, vrf, lazyBind) {}
  private:
  void mqPollThread() override;
};

}
