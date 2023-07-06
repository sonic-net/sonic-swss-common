#pragma once

namespace swss {

class RedisString {
public:

    RedisString();

    ~RedisString();

    void reset(char *str = nullptr, int len = 0);

    const char *getStr() const;

    int getLen() const;

private:
    char *m_data;
    int m_len;
};

}
