AC_INIT([sonic-swss],[1.0])
AC_CONFIG_SRCDIR([])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE
AC_LANG_C
AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_HEADER_STDC

AC_CHECK_LIB([hiredis], [redisConnect])
AC_CHECK_LIB([nl-genl-3], [genl_connect])

AC_ARG_ENABLE(debug,
[  --enable-debug      Compile with debugging flags],
[case "${enableval}" in
	yes) debug=true ;;
	no)  debug=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac],[debug=false])
AM_CONDITIONAL(DEBUG, test x$debug = xtrue)


AC_ARG_WITH(gtest,
[  --with-gtest=<dir>   Compiles tests using the location of google tests],
AC_MSG_NOTICE(Using gtest path from:$with_gtest), with_gtest="none")

GTEST_PATH=
if test "x$with_gtest" != xnone; then
GTEST_PATH=$with_gtest
fi
AC_SUBST(GTEST_PATH)
AM_CONDITIONAL(TESTS, test "x$with_gtest" != xnone)


CFLAGS_COMMON="-std=c++11 -Wall -fPIC -Wno-write-strings -I/usr/include/libnl3"
AC_SUBST(CFLAGS_COMMON)

AC_CONFIG_FILES([
    Makefile
    common/Makefile
    tests/Makefile
])

AC_OUTPUT
