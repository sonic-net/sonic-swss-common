#pragma once
#include <map>
#include <memory>

namespace swss {

class CancellationToken;

/*
    SignalHandlerHelper class provide a native signal handler.
    Python signal handler have following issue:
        A long-running calculation implemented purely in C (such as regular expression matching on a large body of text) may run uninterrupted for an arbitrary amount of time, regardless of any signals received. The Python signal handlers will be called when the calculation finishes.
    For more information, please check: https://docs.python.org/3/library/signal.html
*/
class SignalHandlerHelper
{
public:
    static void registerSignalHandler(int signalNumber);
    static void registerCancellationToken(int signalNumber, std::shared_ptr<CancellationToken> token);
    static void onSignal(int signalNumber);
    
private:
    static std::map<int, std::shared_ptr<CancellationToken> > signalTokenMapping;
};

}