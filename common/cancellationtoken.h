#pragma once

namespace swss {

class CancellationToken
{
public:
    CancellationToken() noexcept;
    bool isCancled() const noexcept;
    void cancel() noexcept;
    void reset() noexcept;

private:
    bool m_cancled;
};

}