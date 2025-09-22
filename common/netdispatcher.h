#pragma once

#include "netmsg.h"

#include <netlink/msg.h>

#include <map>
#include <mutex>

namespace swss
{
    class NetDispatcher
    {
        public:

            /** Get singleton instance. */
            static NetDispatcher& getInstance();

            /**
             * Register callback class according to message-type.
             *
             * Throw exception if callback is already registered.
             */
            void registerMessageHandler(int nlmsg_type, NetMsg *callback);

            /*
             * Register Raw Msg callback class according to message-type.
             *
             * Throw exception if callback is already registered
             */
            void registerRawMessageHandler(int nlmsg_type, NetMsg *callback);

            /*
             * Unregister Raw Msg callback class according to message-type.
             *
             * Throw exception if callback is not registered
             */
            void unregisterRawMessageHandler(int nlmsg_type);


            /** Called by NetLink or FpmLink classes as indication of new packet arrival. */
            void onNetlinkMessage(struct nl_msg *msg);

            /*
             * Called by NetLink or FpmLink classes as indication of new packet arrival
             */
            void onNetlinkMessageRaw(struct nl_msg *msg);

            /**
             * Unregister callback according to message-type.
             *
             * Throw exception if callback is not registered.
             */
            void unregisterMessageHandler(int nlmsg_type);

        private:

            NetDispatcher() = default;

            NetDispatcher(const NetDispatcher&) = delete;

            NetMsg* getCallback(int nlmsg_type);

        private:

            /** nl_msg_parse callback API */
            static void nlCallback(struct nl_object *obj, void *context);

            std::map<int, NetMsg*> m_handlers;

            std::map<int, NetMsg * > m_rawhandlers;

            /** Mutex protecting register, unregister and get callback methods. */
            std::mutex m_mutex;
    };
}
