pkgpython_PYTHON = pyext/py2/swsscommon.py pyext/py2/__init__.py
pkgpyexec_LTLIBRARIES = pyext/py2/_swsscommon.la

pyext_py2__swsscommon_la_SOURCES = pyext/py2/swsscommon_wrap.cpp
pyext_py2__swsscommon_la_CPPFLAGS = -std=c++11 -Icommon -I/usr/include/python$(PYTHON_VERSION) -fstack-protector-strong
pyext_py2__swsscommon_la_LDFLAGS = -module -Wl,-z,now
pyext_py2__swsscommon_la_LIBADD = common/libswsscommon.la -lpython$(PYTHON_VERSION)

if YANGMODS
SWIG_FLAG += -DENABLE_YANG_MODULES
pyext_py2__swsscommon_la_CPPFLAGS += -DENABLE_YANG_MODULES
endif

pyext/py2/swsscommon_wrap.cpp: $(SWIG_SOURCES)
	$(SWIG) $(SWIG_FLAG) -Icommon -o $@ $<

CLEANFILES += pyext/py2/swsscommon_wrap.cpp pyext/py2/swsscommon.py
