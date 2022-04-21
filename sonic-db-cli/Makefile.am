###########################################################################
##
## File:        ./Makefile.am
## Versions:    $Id: Makefile.am,v 1.0 2022/04/02 12:04:29 liuh@microsoft.com Exp $
## Created:     2022/04/02
##
###########################################################################
ACLOCAL_AMFLAGS = -I m4 --install
ACLOCAL_AMFLAGS = -I config
AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = tests

bin_PROGRAMS = sonic-db-cli
sonic_db_cli_SOURCES = sonic-db-cli.h sonic-db-cli.cpp
sonic_db_cli_CPPFLAGS = $(AM_CPPFLAGS) -std=c++17
sonic_db_cli_LDFLAGS = -lswsscommon $(BOOST_PROGRAM_OPTIONS_LIB)

EXTRA_DIST = bootstrap sonic-db-cli.spec

MAINTAINERCLEANFILES = Makefile.in config.h.in configure aclocal.m4 \
                       config/config.guess  config/config.sub  config/depcomp \
                       config/install-sh config/ltmain.sh config/missing

pkgconfigdir = $(libdir)/pkgconfig
