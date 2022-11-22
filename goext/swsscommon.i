%module swsscommon

%{
#include "dbconnector.h"
#include "producerstatetable.h"
using namespace swss;
%}

%include "std_string.i"

%include "dbconnector.h"
%include "producerstatetable.h"

