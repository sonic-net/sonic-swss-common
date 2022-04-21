#INCLUDES = -I $(top_srcdir)

bin_PROGRAMS = tests
DBGFLAGS = -ggdb -DDEBUG

CFLAGS_GTEST =  -DTESTING  -Wno-write-strings
LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main -lgmock -lgmock_main

tests_SOURCES = main.cpp \
                ../sonic-db-cli.cpp \
                ../sonic-db-cli.h

tests_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST)
tests_CPPFLAGS = $(tests_CFLAGS)  -std=c++17
tests_LDADD = $(LDADD_GTEST) -lpthread -lswsscommon $(CODE_COVERAGE_LIBS) $(BOOST_PROGRAM_OPTIONS_LIB)