INCLUDES = -I $(top_srcdir)

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g -DNDEBUG
endif

bin_PROGRAMS = sonic-db-cli
sonic_db_cli_SOURCES = sonic-db-cli.h sonic-db-cli.cpp
sonic_db_cli_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
sonic_db_cli_LDFLAGS = -L$(top_srcdir)/common -lswsscommon $(BOOST_PROGRAM_OPTIONS_LIB)
