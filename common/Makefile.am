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
	dbconnector.cpp

libswsscommon_la_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
libswsscommon_la_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
libswsscommon_la_LIBADD = -lnl-3 -lnl-route-3 -lhiredis
