#include <signal.h>

#include "signalhandlerhelper.h"

using namespace swss;

std::map<int, bool> SignalHandlerHelper::signalStatusMapping;

void SignalHandlerHelper::registerSignalHandler(int signalNumber)
{
    signalStatusMapping[signalNumber] = false;
    signal(signalNumber, SignalHandlerHelper::onSignal);
}

void SignalHandlerHelper::onSignal(int signalNumber)
{
    signalStatusMapping[signalNumber] = true;
}

bool SignalHandlerHelper::checkSignal(int signalNumber)
{
    auto result = signalStatusMapping.find(signalNumber);
    if (result != signalStatusMapping.end()) {
        return result->second;
    }

    return false;
}

void SignalHandlerHelper::resetSignal(int signalNumber)
{
    signalStatusMapping[signalNumber] = false;
}