#include <signal.h>

#include "cancellationtoken.h"
#include "signalhandlerhelper.h"

using namespace swss;

std::map<int, std::shared_ptr<CancellationToken> > SignalHandlerHelper::signalTokenMapping;

void SignalHandlerHelper::registerSignalHandler(int signalNumber)
{
    signal(signalNumber, SignalHandlerHelper::onSignal);
}

void SignalHandlerHelper::registerCancellationToken(int signalNumber, std::shared_ptr<CancellationToken> token)
{
    signalTokenMapping[signalNumber] = token;
}

void SignalHandlerHelper::onSignal(int signalNumber)
{
    auto result = signalTokenMapping.find(signalNumber);
    if (result != signalTokenMapping.end())
    {
        result->second->cancel();
    }
}

