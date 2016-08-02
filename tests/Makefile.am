INCLUDES = -I $(top_srcdir)

bin_PROGRAMS = tests

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

CFLAGS_GTEST = -I $(top_srcdir)/googletest/googletest/include
LDADD_GTEST = $(top_srcdir)/googletest/build/googlemock/gtest/libgtest_main.a \
        $(top_srcdir)/googletest/build/googlemock/gtest/libgtest.a

tests_SOURCES = redis_ut.cpp    \
                tokenize_ut.cpp

tests_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST)
tests_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST)
tests_LDADD = $(LDADD_GTEST) -lnl-genl-3 -lhiredis -lhiredis -lpthread \
        -L$(top_srcdir)/common -lswsscommon

