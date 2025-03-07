lib_LTLIBRARIES =
bin_PROGRAMS =

if DEBUG
DBGFLAGS = -ggdb -DDEBUG -gdwarf-5
else
DBGFLAGS = -g -DNDEBUG
endif

SWIG_SOURCES = pyext/swsscommon.i
SWIG_FLAG = -Wall -c++ -python -keyword
if ARCH64
SWIG_FLAG += -DSWIGWORDSIZE64
endif

CLEANFILES =

include common/Makefile.am
include pyext/Makefile.am
include sonic-db-cli/Makefile.am
include tests/Makefile.am


ACLOCAL_AMFLAGS = -I m4

# Add to BUILT_SOURCES to ensure it's built first
BUILT_SOURCES = common/cfg_schema.h

# Add rule to generate the header file
common/cfg_schema.h: gen_cfg_schema.py
	$(AM_V_GEN)python $(srcdir)/gen_cfg_schema.py -o $@

# Make sure the header is cleaned up
CLEANFILES += common/cfg_schema.h
