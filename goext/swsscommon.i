%module swsscommon

%{
#include <string>
#include "schema.h"
#include "dbconnector.h"
#include "select.h"
#include "selectable.h"
#include "rediscommand.h"
#include "table.h"
#include "redispipeline.h"
#include "redisselect.h"
#include "redistran.h"
#include "producerstatetable.h"
#include "consumertablebase.h"
#include "consumerstatetable.h"
#include "subscriberstatetable.h"
#include "notificationconsumer.h"
%}

%include <typemaps.i>
%include "std_string.i"
%include "std_vector.i"
%include <std_pair.i>

namespace std {
	%template(StringPair) std::pair<std::string, std::string>;
	%template(FieldValuePairs) std::vector<std::pair<std::string, std::string>>;
	%template(VectorString) std::vector<std::string>;
}

%apply int *OUTPUT {int *fd};

/* Ignoring this input argument, but using local temp */
%typemap(in, numinputs=0) swss::Selectable ** (swss::Selectable *temp) {
    $1 = &temp;
}

/* Have not figured out a way to have the argout data returned in $result for golang */
/*
%typemap(argout) swss::Selectable ** {
    PyObject* temp = NULL;
    if (!PyList_Check($result)) {
        temp = $result;
        $result = PyList_New(1);
        PyList_SetItem($result, 0, temp);
    }
    temp = SWIG_NewPointerObj(*$1, SWIGTYPE_p_swss__Selectable, SWIG_POINTER_OWN);
    PyList_Append($result, temp);
}
*/

%include "schema.h"
%include "dbconnector.h"
%include "selectable.h"
%include "select.h"
%include "rediscommand.h"
%include "redispipeline.h"
%include "redisselect.h"
%include "redistran.h"

%include "table.h"

%include "producerstatetable.h"

/*
 * Not taking effect,  it crashes if non-const string reference is passed down.
 */
%apply std::string& OUTPUT {std::string &key};
%apply std::string& OUTPUT {std::string &op};

%include "consumertablebase.h"
%clear std::string &key;
%clear std::string &op;

%include "consumerstatetable.h"
%include "subscriberstatetable.h"
%include "notificationconsumer.h"
