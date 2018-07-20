SWIG_SOURCES = swsscommon.i

pkgpython_PYTHON = swsscommon.py __init__.py
pkgpyexec_LTLIBRARIES = _swsscommon.la

_swsscommon_la_SOURCES = swsscommon_wrap.cpp
_swsscommon_la_CPPFLAGS = -std=c++11 -I../common -I/usr/include/python$(PYTHON_VERSION)
_swsscommon_la_LDFLAGS = -module
_swsscommon_la_LIBADD = ../common/libswsscommon.la -lpython$(PYTHON_VERSION)

swsscommon_wrap.cpp: $(SWIG_SOURCES)
	$(SWIG) -c++ -python -I../common -o $@ $<

CLEANFILES = swsscommon_wrap.cpp
