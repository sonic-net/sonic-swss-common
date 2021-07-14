AC_INIT([swsscommon],[1.0])
AC_CONFIG_SRCDIR([])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AC_LANG_C
AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_HEADER_STDC
AM_PATH_PYTHON
AM_PATH_PYTHON3

AC_CHECK_LIB([hiredis], [redisConnect])

LDFLAGS="-Wl,--no-undefined $LDFLAGS"
AC_SUBST([LDFLAGS])

AC_ARG_ENABLE(debug,
[  --enable-debug       Compile with debugging flags],
[case "${enableval}" in
	yes) debug=true ;;
	no)  debug=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac],[debug=false])
AM_CONDITIONAL(DEBUG, test x$debug = xtrue)
AM_CONDITIONAL(ARCH64, test `getconf LONG_BIT` = "64")

AC_ARG_WITH(libnl-3.0-inc,
[  --with-libnl-3.0-inc=DIR
                           prefix where libnl-3.0 includes are installed],
[AC_SUBST(CPPFLAGS, "$CPPFLAGS -I${withval}")
 AC_SUBST(LIBNL_INC_DIR, "${withval}")])

AC_ARG_WITH(libnl-3.0-lib,
[  --with-libnl-3.0-lib=DIR
                           prefix where libnl-3.0 libraries are installed],
[AC_SUBST(LDFLAGS, "$LDFLAGS -L${withval}")
 AC_SUBST(LIBNL_LIB_DIR, "${withval}")])

AC_ARG_WITH(libnl-3.0-usr-lib,
[  --with-libnl-3.0-usr-lib=DIR
                           prefix where libnl-3.0 libraries are installed],
[AC_SUBST(LDFLAGS, "$LDFLAGS -L${withval}")
 AC_SUBST(LIBNL_USR_LIB_DIR, "${withval}")])

if test "${with_libnl_3_0_inc+set}" = set; then
    AC_CHECK_LIB([nl-3], [nl_addr_alloc])
    AC_CHECK_LIB([nl-genl-3], [nl_socket_get_cb])
    AC_CHECK_LIB([nl-route-3], [nl_object_alloc])
    AC_CHECK_LIB([nl-nf-3], [nfnl_connect])
else
    PKG_CHECK_MODULES([LIBNL], [libnl-3.0 libnl-genl-3.0 libnl-route-3.0 libnl-nf-3.0])
            CFLAGS="$CFLAGS $LIBNL_CFLAGS"
            LIBS="$LIBS $LIBNL_LIBS"
fi

AC_PATH_PROG(SWIG, [swig3.0])

CFLAGS_COMMON=""
CFLAGS_COMMON+=" -ansi"
CFLAGS_COMMON+=" -fPIC"
CFLAGS_COMMON+=" -std=c++11"
CFLAGS_COMMON+=" -Wall"
CFLAGS_COMMON+=" -Wcast-align"
CFLAGS_COMMON+=" -Wcast-qual"
CFLAGS_COMMON+=" -Wconversion"
CFLAGS_COMMON+=" -Wdisabled-optimization"
CFLAGS_COMMON+=" -Werror"
CFLAGS_COMMON+=" -Wextra"
CFLAGS_COMMON+=" -Wfloat-equal"
CFLAGS_COMMON+=" -Wformat=2"
CFLAGS_COMMON+=" -Wformat-nonliteral"
CFLAGS_COMMON+=" -Wformat-security"
CFLAGS_COMMON+=" -Wformat-y2k"
CFLAGS_COMMON+=" -Wimport"
CFLAGS_COMMON+=" -Winit-self"
CFLAGS_COMMON+=" -Winvalid-pch"
CFLAGS_COMMON+=" -Wlong-long"
CFLAGS_COMMON+=" -Wmissing-field-initializers"
CFLAGS_COMMON+=" -Wmissing-format-attribute"
CFLAGS_COMMON+=" -Wmissing-include-dirs"
CFLAGS_COMMON+=" -Wmissing-noreturn"
CFLAGS_COMMON+=" -Wno-aggregate-return"
CFLAGS_COMMON+=" -Wno-padded"
CFLAGS_COMMON+=" -Wno-switch-enum"
CFLAGS_COMMON+=" -Wno-unused-parameter"
CFLAGS_COMMON+=" -Wpacked"
CFLAGS_COMMON+=" -Wpointer-arith"
CFLAGS_COMMON+=" -Wredundant-decls"
CFLAGS_COMMON+=" -Wshadow"
CFLAGS_COMMON+=" -Wstack-protector"
CFLAGS_COMMON+=" -Wstrict-aliasing=3"
CFLAGS_COMMON+=" -Wswitch"
CFLAGS_COMMON+=" -Wswitch-default"
CFLAGS_COMMON+=" -Wunreachable-code"
CFLAGS_COMMON+=" -Wunused"
CFLAGS_COMMON+=" -Wvariadic-macros"
CFLAGS_COMMON+=" -Wno-write-strings"
CFLAGS_COMMON+=" -Wno-missing-format-attribute"
CFLAGS_COMMON+=" -Wno-long-long"

AC_SUBST(CFLAGS_COMMON)

AC_CONFIG_FILES([
    Makefile
    common/Makefile
    pyext/Makefile
    pyext/py2/Makefile
    pyext/py3/Makefile
    tests/Makefile
])

AC_OUTPUT
