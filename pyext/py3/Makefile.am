SWIG_SOURCES = ../swsscommon.i
SWIG_SIGNAL_SOURCES = ../signal.i

pkgpython3_PYTHON = swsscommon.py __init__.py signal.py
pkgpy3exec_LTLIBRARIES = _swsscommon.la _signal.la

_swsscommon_la_SOURCES = swsscommon_wrap.cpp
_swsscommon_la_CPPFLAGS = -std=c++11 -I../../common -I/usr/include/python$(PYTHON3_VERSION)
_swsscommon_la_LDFLAGS = -module
_swsscommon_la_LIBADD = ../../common/libswsscommon.la $(PYTHON3_BLDLIBRARY)

_signal_la_SOURCES = signal_wrap.cpp
_signal_la_CPPFLAGS = -std=c++11 -I../../common -I/usr/include/python$(PYTHON3_VERSION)
_signal_la_LDFLAGS = -module
_signal_la_LIBADD = ../../common/libswsscommon.la $(PYTHON3_BLDLIBRARY)

SWIG_FLAG = -Wall -c++ -python -keyword
if ARCH64
SWIG_FLAG += -DSWIGWORDSIZE64
endif

swsscommon_wrap.cpp: $(SWIG_SOURCES)
	$(SWIG) $(SWIG_FLAG) -I../../common -o $@ $<

signal_wrap.cpp: $(SWIG_SIGNAL_SOURCES)
	$(SWIG) $(SWIG_FLAG) -threads -I../../common -o $@ $<

CLEANFILES = swsscommon_wrap.cpp signal_wrap.cpp
