#pragma once
#include <map>
#include <memory>
#include <signal.h>

namespace swss {

typedef std::pair<struct sigaction, struct sigaction> SigActionPair;

// Define signal ID enum for python
enum Signals
{
    SIGNAL_TERM = SIGTERM,
    SIGNAL_INT = SIGINT
};

class SignalCallbackBase
{
   public:
      virtual ~SignalCallbackBase() {};
      virtual void onSignal(int signalNumber) = 0; 
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
    static void registerSignalHandler(int signalNumber, std::shared_ptr<SignalCallbackBase> callback);
    static void restoreSignalHandler(int signalNumber);
    static void onSignal(int signalNumber);
    static bool checkSignal(int signalNumber);
    static void resetSignal(int signalNumber);
    
private:
    static std::map<int, bool> m_signalStatusMapping;
    static std::map<int, SigActionPair> m_sigActionMapping;
    static std::map<int, std::shared_ptr<SignalCallbackBase>> m_sigCallbackMapping;
};

/*
    Register python signal handler to swsscommon.
*/
#ifdef SWIG
%pythoncode %{
def RegisterSignal(signalNumber, handler):
    if not callable(handler):
        raise TypeError("Parameter handler is not a callable object!")

    class SignalCallbackWrapper(SignalCallbackBase):
        def __init__(self, signalhandler):
            super(SignalCallbackWrapper, self).__init__()
            self.m_signalhandler = signalhandler

        def onSignal(self, signalNumber):
            # Call signal handler from c++, there is no python stack, always pass stack frame object as None
            self.m_signalhandler(signalNumber, None)

    warpper = SignalCallbackWrapper(handler)

    # Transfer ownership to SignalHandlerHelper, for more information please check: https://www.swig.org/Doc4.0/Python.html#Python_nn35
    SignalHandlerHelper.registerSignalHandler(signalNumber, warpper.__disown__())

%}
#endif

}