INCLUDES = -I $(top_srcdir)

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g -DNDEBUG
endif

lib_LTLIBRARIES = libsonicdbcli.la
libsonicdbcli_la_SOURCES = sonic-db-cli.cpp
libsonicdbcli_la_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
libsonicdbcli_la_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
libsonicdbcli_la_LDFLAGS = -L$(top_srcdir)/common -lswsscommon -lpthread

bin_PROGRAMS = sonic-db-cli
sonic_db_cli_SOURCES = sonic-db-cli.cpp main.cpp
sonic_db_cli_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
sonic_db_cli_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
sonic_db_cli_LDFLAGS = -L$(top_srcdir)/common -lswsscommon -lpthread
