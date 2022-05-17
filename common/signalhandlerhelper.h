#pragma once
#include <map>
#include <signal.h>

namespace swss {

typedef std::pair<struct sigaction, struct sigaction> SigActionPair;

// Define signal ID enum for python
enum Signals
{
    SIGNAL_TERM = SIGTERM,
    SIGNAL_INT = SIGINT
};

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
    static void restoreSignalHandler(int signalNumber);
    static void onSignal(int signalNumber);
    static bool checkSignal(int signalNumber);
    static void resetSignal(int signalNumber);
    
private:
    static std::map<int, bool> m_signalStatusMapping;
    static std::map<int, SigActionPair> m_sigActionMapping;
};

}