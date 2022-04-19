#include <signal.h>

#include "cancellationtoken.h"
#include "signalhandlerhelper.h"

using namespace swss;

std::map<int, CancellationToken*> SignalHandlerHelper::signalTokenMapping;

void SignalHandlerHelper::RegisterSignalHandler(int signalNumber)
{
    signal(signalNumber, SignalHandlerHelper::OnSignal);
}

void SignalHandlerHelper::RegisterCancellationToken(int signalNumber, CancellationToken& token)
{
    signalTokenMapping[signalNumber] = &token;
}

void SignalHandlerHelper::OnSignal(int signalNumber)
{
    auto result = signalTokenMapping.find(signalNumber);
    if (result != signalTokenMapping.end())
    {
        result->second->Cancel();
    }
}

