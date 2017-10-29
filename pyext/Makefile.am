SWIG_SOURCES = swsscommon.i

pkgpython_PYTHON = swsscommon.py __init__.py
pkgpyexec_LTLIBRARIES = _swsscommon.la

_swsscommon_la_SOURCES = swsscommon_wrap.cpp
_swsscommon_la_CPPFLAGS = -std=c++11 -I../common $(AX_SWIG_PYTHON_CPPFLAGS)
_swsscommon_la_LDFLAGS = -module
_swsscommon_la_LIBADD = ../common/libswsscommon.la $(PYTHON_LIBS)

swsscommon_wrap.cpp: $(SWIG_SOURCES)
	$(SWIG) -python -I../common -o $@ $<
