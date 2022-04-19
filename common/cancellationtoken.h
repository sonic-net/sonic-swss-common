#pragma once

namespace swss {

class CancellationToken
{
public:
    CancellationToken();
    ~CancellationToken();
    bool IsCancled();
    void Cancel();
    void Reset();

private:
    bool m_cancled;
};

}