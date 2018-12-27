INCLUDES = -I $(top_srcdir)

bin_PROGRAMS = tests

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g -DNDEBUG
endif

CFLAGS_GTEST =
LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = redis_ut.cpp                \
                redis_piped_ut.cpp          \
                redis_state_ut.cpp          \
                redis_piped_state_ut.cpp    \
                tokenize_ut.cpp             \
                json_ut.cpp                 \
                ntf_ut.cpp                  \
                ipaddress_ut.cpp            \
                ipprefix_ut.cpp             \
                macaddress_ut.cpp           \
                converter_ut.cpp            \
                exec_ut.cpp                 \
                redis_subscriber_state_ut.cpp \
                selectable_priority.cpp       \
                warm_restart_ut.cpp

tests_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST) $(LIBNL_CFLAGS)
tests_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST) $(LIBNL_CFLAGS)
tests_LDADD = $(LDADD_GTEST) -lpthread -L$(top_srcdir)/common -lswsscommon $(LIBNL_LIBS)
