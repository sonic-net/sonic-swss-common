#pragma once

namespace swss {

class CancellationToken
{
public:
    CancellationToken();
    ~CancellationToken();
    bool IsCancled();
    void Cancel();

private:
    bool m_cancled;
};

}