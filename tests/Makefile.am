INCLUDES = -I $(top_srcdir)

bin_PROGRAMS = tests

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

CFLAGS_GTEST = -I $(GTEST_PATH)/googletest/include
LDADD_GTEST = $(GTEST_PATH)/build/googletest/libgtest_main.a \
			  $(GTEST_PATH)/build/googletest/libgtest.a

tests_SOURCES = redis_ut.cpp

tests_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST)
tests_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST)
tests_LDADD = -lnl-genl-3 -lhiredis -lhiredis -lpthread \
	-L$(top_srcdir)/common -lswsscommon $(LDADD_GTEST)

