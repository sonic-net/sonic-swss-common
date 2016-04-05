INCLUDES = -I $(top_srcdir)

lib_LTLIBRARIES = libswsscommon.la

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

libswsscommon_la_SOURCES = \
    logger.cpp                \
    redisreply.cpp            \
    dbconnector.cpp           \
    table.cpp                 \
    counter.cpp               \
    json.cpp                  \
    producertable.cpp         \
    select.cpp                \
    consumertable.cpp         \
    ipaddress.cpp             \
    ipprefix.cpp              \
    ipaddresses.cpp           \
    macaddress.cpp            \
    netdispatcher.cpp         \
    netlink.cpp               \
    linkcache.cpp

libswsscommon_la_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
libswsscommon_la_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
libswsscommon_la_LIBADD = -lnl-3 -lnl-route-3 -lhiredis
