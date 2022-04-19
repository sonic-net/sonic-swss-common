#pragma once
#include <map>

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
    static void RegisterSignalHandler(int signalNumber);
    static void RegisterCancellationToken(int signalNumber, CancellationToken& token);
    static void OnSignal(int signalNumber);
    
private:
    static std::map<int, CancellationToken*> signalTokenMapping;
};

}