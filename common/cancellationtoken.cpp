#include "cancellationtoken.h"

using namespace swss;

CancellationToken::CancellationToken() noexcept
:m_cancled(false)
{
}

bool CancellationToken::isCancled() const noexcept
{
    return m_cancled;
}

void CancellationToken::cancel() noexcept
{
    m_cancled = true;
}

void CancellationToken::reset() noexcept
{
    m_cancled = false;
}

