#include "cancellationtoken.h"

using namespace swss;

CancellationToken::CancellationToken()
:m_cancled(false)
{
}

CancellationToken::~CancellationToken()
{
    Cancel();
}

bool CancellationToken::IsCancled()
{
    return m_cancled;
}

void CancellationToken::Cancel()
{
    m_cancled = true;
}

void CancellationToken::Reset()
{
    m_cancled = false;
}

