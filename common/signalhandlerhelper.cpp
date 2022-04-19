#include <map>
#include <memory>

#include <signal.h>

#include "cancellationtoken.h"
#include "signalhandlerhelper.h"

using namespace std;
using namespace swss;

void SignalHandlerHelper::RegisterSignalHandler(int signalNumber)
{
    signal(signalNumber, SignalHandlerHelper::OnSignal);
}

CancellationToken &SignalHandlerHelper::GetCancellationToken(int signalNumber)
{
    static map<int, shared_ptr<CancellationToken>> signalTokenMapping;

    auto result = signalTokenMapping.find(signalNumber);
    if (result != signalTokenMapping.end())
    {
        return *(result->second.get());
    }

    auto newToken = make_shared<CancellationToken>();
    signalTokenMapping.emplace(signalNumber, newToken);
    return *(newToken.get());
}

void SignalHandlerHelper::OnSignal(int signalNumber)
{
    CancellationToken &token = GetCancellationToken(signalNumber);
    token.Cancel();
}

