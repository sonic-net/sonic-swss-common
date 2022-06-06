%module signal
// Enable directors feature for signal callback
// Enable threads feature, because signal handler need cross threads.
%module(directors="1","threads"=1) signal

%rename(delete) del;

%{
// ref: http://www.swig.org/Doc3.0/Python.html
// A Python 2 string is not a unicode string by default and should a Unicode
// string be passed to C/C++ it will fail to convert to a C/C++ string (char *
// or std::string types). The Python 2 behavior can be made more like Python 3
// by defining SWIG_PYTHON_2_UNICODE when compiling the generated C/C++ code.
// Unicode strings will be successfully accepted and converted from UTF-8, but
// note that they are returned as a normal Python 2 string
#define SWIG_PYTHON_2_UNICODE

#include "signalhandlerhelper.h"
%}

%include <std_string.i>
%include <std_map.i>
%include <std_shared_ptr.i>
%include <typemaps.i>
%include <stdint.i>
%include <exception.i>

%shared_ptr(swss::SignalCallbackBase)
%feature("director") swss::SignalCallbackBase;
%include "signalhandlerhelper.h"
