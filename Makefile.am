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

# This will set redis_acl to an empty value if not already set
redis_acl ?=

# If redis_acl is set, append -D$(redis_acl) to DBGFLAGS
ifneq ($(strip $(redis_acl)),)
ifeq ($(redis_acl),y)
REDIS_ACL_G=1
else ifeq ($(redis_acl),n)
REDIS_ACL_G=0
endif
AM_CPPFLAGS += -DREDIS_ACL_G
endif

ACLOCAL_AMFLAGS = -I m4
