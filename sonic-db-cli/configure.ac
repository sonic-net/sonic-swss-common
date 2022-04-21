dnl
dnl File:        configure.in
dnl Revision:    $Id: configure.ac,v 1.0 2022/04/02 12:04:29 liuh@microsoft.com Exp $
dnl Created:     2022/04/02
dnl Author:      Liu Hua <liuh@microsoft.com>
dnl
dnl Process this file with autoconf to produce a configure script
dnl You need autoconf 2.59 or better!
dnl
dnl ---------------------------------------------------------------------------

AC_PREREQ(2.59)
AC_COPYRIGHT([
See the included file: COPYING for copyright information.
])
AC_INIT(sonic-db-cli, 1.0.0, [liuh@microsoft.com])

AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([sonic-db-cli.cpp])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

dnl --------------------------------------------------------------------
dnl Checks for programs.
AC_LANG_C
AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_ENABLE_SHARED
AC_DISABLE_STATIC
AM_PROG_LIBTOOL

dnl --------------------------------------------------------------------
dnl Checks for libraries.
AC_CHECK_LIB(swsscommon, conn)
dnl Check boost libs: https://www.gnu.org/software/autoconf-archive/The-Macros.html#The-Macros
AX_BOOST_BASE(,, [AC_MSG_ERROR([boost lib missing])])
AX_BOOST_PROGRAM_OPTIONS

dnl --------------------------------------------------------------------
dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADER([swss/sonicv2connector.h], [], [AC_MSG_ERROR([swsscommon lib missing. ])] )

dnl --------------------------------------------------------------------
dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl --------------------------------------------------------------------
dnl Checks for library functions.
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([bzero gethostbyname gettimeofday inet_ntoa select socket logwtmp getrandom])

dnl --------------------------------------------------------------------
dnl Generate made files
AC_CONFIG_FILES([
    Makefile
    tests/Makefile
])
AC_OUTPUT
