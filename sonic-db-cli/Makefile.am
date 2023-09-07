lib_LTLIBRARIES += sonic-db-cli/libsonicdbcli.la
sonic_db_cli_libsonicdbcli_la_SOURCES = sonic-db-cli/sonic-db-cli.cpp
sonic_db_cli_libsonicdbcli_la_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
sonic_db_cli_libsonicdbcli_la_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
sonic_db_cli_libsonicdbcli_la_LIBADD = common/libswsscommon.la -lpthread
sonic_db_cli_libsonicdbcli_la_LDFLAGS = -Wl,-z,now $(LDFLAGS)

bin_PROGRAMS += sonic-db-cli/sonic-db-cli
sonic_db_cli_sonic_db_cli_SOURCES = sonic-db-cli/sonic-db-cli.cpp sonic-db-cli/main.cpp
sonic_db_cli_sonic_db_cli_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON)
sonic_db_cli_sonic_db_cli_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
sonic_db_cli_sonic_db_cli_LDADD = common/libswsscommon.la -lpthread
sonic_db_cli_sonic_db_cli_LDFLAGS = -Wl,-z,now $(LDFLAGS)
