INCLUDES = -I $(top_srcdir) -I/usr/include/libnl3

lib_LTLIBRARIES = libswsscommon.la

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
    select.cpp                \
    selectableevent.cpp       \
    consumertable.cpp         \
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
    tokenize.cpp

libswsscommon_la_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
libswsscommon_la_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
libswsscommon_la_LIBADD =
