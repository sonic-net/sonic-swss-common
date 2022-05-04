INCLUDES = -I $(top_srcdir)

bin_PROGRAMS = tests tests_asan tests_tsan tests_usan

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g -DNDEBUG
endif

CFLAGS_GTEST =
LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main -lgmock -lgmock_main
CFLAGS_ASAN = -fsanitize=address -static-libasan
CFLAGS_TSAN = -fsanitize=thread
CFLAGS_USAN = -fsanitize=undefined

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
                warm_restart_ut.cpp         \
                redis_multi_db_ut.cpp       \
                logger_ut.cpp               \
                redis_multi_ns_ut.cpp       \
                fdb_flush.cpp               \
                stringutility_ut.cpp        \
                redisutility_ut.cpp         \
                boolean_ut.cpp              \
                status_code_util_test.cpp   \
                saiaclschema_ut.cpp         \
                main.cpp

tests_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST) $(LIBNL_CFLAGS)
tests_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST) $(LIBNL_CFLAGS)
tests_LDADD = $(LDADD_GTEST) -lpthread -L$(top_srcdir)/common -lswsscommon $(LIBNL_LIBS) $(CODE_COVERAGE_LIBS)

tests_asan_SOURCES = $(tests_SOURCES)
tests_asan_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST) $(LIBNL_CFLAGS)
tests_asan_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST) $(LIBNL_CFLAGS)
tests_asan_LDFLAGS = $(CFLAGS_ASAN)
tests_asan_LDADD = $(LDADD_GTEST) -lpthread -L$(top_srcdir)/common -lswsscommon $(LIBNL_LIBS)

tests_tsan_SOURCES = $(tests_SOURCES)
tests_tsan_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST) $(LIBNL_CFLAGS)
tests_tsan_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST) $(LIBNL_CFLAGS)
tests_tsan_LDFLAGS = $(CFLAGS_TSAN)
tests_tsan_LDADD = $(LDADD_GTEST) -lpthread -L$(top_srcdir)/common -lswsscommon $(LIBNL_LIBS)

tests_usan_SOURCES = $(tests_SOURCES)
tests_usan_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST) $(LIBNL_CFLAGS)
tests_usan_CPPFLAGS = $(DBGFLAGS) $(AM_CFLAGS) $(CFLAGS_COMMON) $(CFLAGS_GTEST) $(LIBNL_CFLAGS)
tests_usan_LDFLAGS = $(CFLAGS_USAN)
tests_usan_LDADD = $(LDADD_GTEST) -lpthread -L$(top_srcdir)/common -lswsscommon $(LIBNL_LIBS)
