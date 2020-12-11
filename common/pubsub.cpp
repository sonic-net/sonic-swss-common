#include "pubsub.h"
#include "dbconnector.h"
#include "logger.h"

using namespace std;
using namespace swss;

PubSub::PubSub(const DBConnector& other)
    : DBConnector(other)
{
}

int redisPollReply(redisContext *c, void **reply) {
    int wdone = 0;
    void *aux = NULL;

    /* Try to read pending replies */
    printf("@@before redisGetReplyFromReader()\n");
    if (redisGetReplyFromReader(c,&aux) == REDIS_ERR)
        return REDIS_ERR;
    printf("@@after redisGetReplyFromReader()\n");

    /* For the blocking context, flush output buffer and read reply */
    if (aux == NULL && c->flags & REDIS_BLOCK) {
        /* Write until done */
        do {
            printf("@@before redisBufferWrite()\n");
            int status;

            if ((status = redisBufferWrite(c,&wdone)) == REDIS_ERR)
                return REDIS_ERR;
            printf("@@after redisBufferWrite(): %d, %d\n", status, wdone);
        } while (!wdone);

        /* Read until there is a reply */
        // do {
            printf("@@before redisBufferRead()\n");
            if (redisBufferRead(c) == REDIS_ERR)
                return REDIS_ERR;
            printf("@@before redisGetReplyFromReader(1)\n");
            if (redisGetReplyFromReader(c,&aux) == REDIS_ERR)
                return REDIS_ERR;
            printf("@@after redisGetReplyFromReader(1)\n");
        // } while (aux == NULL);
    }

    /* Set reply object */
    if (reply != NULL) *reply = aux;
    return REDIS_OK;
}

map<string, string> PubSub::get_message()
{
    map<string, string> ret;
    redisReply *reply = nullptr;
    int status;

    status = redisPollReply(getContext(), reinterpret_cast<void**>(&reply));
    if(status != REDIS_OK || reply == nullptr)
    {
        return ret;
    }

    printf("@@after redisPollReply\n");
    RedisReply r(reply);
    printf("@@after RedisReply ctor\n");
    auto message = r.getReply<RedisMessage>();
    printf("@@after RedisReply.getReply()\n");
    ret["type"] = message.type;
    ret["pattern"] = message.pattern;
    ret["channel"] = message.channel;
    ret["data"] = message.data;
    return ret;
}
