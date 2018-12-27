INCLUDES = -I $(top_srcdir)

lib_LTLIBRARIES = libswsscommon.la

EXTRA_DIST = \
    consumer_state_table_pops.lua \
    consumer_table_pops.lua \
    producer_state_table_apply_view.lua \
    table_dump.lua

swssdir = $(datadir)/swss

dist_swss_DATA = $(EXTRA_DIST)

bin_PROGRAMS = swssloglevel

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g -DNDEBUG
endif

libswsscommon_la_SOURCES = \
    logger.cpp                \
    redisreply.cpp            \
    dbconnector.cpp           \
    table.cpp                 \
    json.cpp                  \
    producertable.cpp         \
    producerstatetable.cpp    \
    redisclient.cpp           \
    rediscommand.cpp          \
    redistran.cpp             \
    redisselect.cpp           \
    select.cpp                \
    selectableevent.cpp       \
    selectabletimer.cpp       \
    consumertable.cpp         \
    consumertablebase.cpp     \
    consumerstatetable.cpp    \
    ipaddress.cpp             \
    ipprefix.cpp              \
    ipaddresses.cpp           \
    macaddress.cpp            \
    netdispatcher.cpp         \
    netlink.cpp               \
    notificationconsumer.cpp  \
    notificationproducer.cpp  \
    linkcache.cpp             \
    portmap.cpp               \
    tokenize.cpp              \
    exec.cpp                  \
    subscriberstatetable.cpp  \
    timestamp.cpp             \
    warm_restart.cpp

libswsscommon_la_CXXFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(LIBNL_CFLAGS)
libswsscommon_la_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(LIBNL_CPPFLAGS)
libswsscommon_la_LIBADD = -lpthread $(LIBNL_LIBS)

swssloglevel_SOURCES = loglevel.cpp

swssloglevel_CXXFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
swssloglevel_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
swssloglevel_LDADD = libswsscommon.la
