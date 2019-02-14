%module swsscommon

%{
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
#include "producertable.h"
#include "consumertable.h"
#include "subscriberstatetable.h"
#include "notificationconsumer.h"
#include "notificationproducer.h"
#include "warm_restart.h"
%}

%include <std_string.i>
%include <std_vector.i>
%include <std_pair.i>
%include <typemaps.i>
%include <stdint.i>

%template(FieldValuePair) std::pair<std::string, std::string>;
%template(FieldValuePairs) std::vector<std::pair<std::string, std::string>>;
%template(VectorString) std::vector<std::string>;

%apply int *OUTPUT {int *fd};
%typemap(in, numinputs=0) swss::Selectable ** (swss::Selectable *temp) {
    $1 = &temp;
}
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
%include "schema.h"
%include "dbconnector.h"
%include "selectable.h"
%include "select.h"
%include "rediscommand.h"
%include "redispipeline.h"
%include "redisselect.h"
%include "redistran.h"

%apply std::vector<std::string>& OUTPUT {std::vector<std::string> &keys};
%apply std::vector<std::pair<std::string, std::string>>& OUTPUT {std::vector<std::pair<std::string, std::string>> &ovalues};
%include "table.h"
%clear std::vector<std::string> &keys;
%clear std::vector<std::pair<std::string, std::string>> &values;

%include "producertable.h"
%include "producerstatetable.h"

%apply std::string& OUTPUT {std::string &key};
%apply std::string& OUTPUT {std::string &op};
%apply std::vector<std::pair<std::string, std::string>>& OUTPUT {std::vector<std::pair<std::string, std::string>> &fvs};
%include "consumertablebase.h"
%clear std::string &key;
%clear std::string &op;
%clear std::vector<std::pair<std::string, std::string>> &fvs;

%include "consumertable.h"
%include "consumerstatetable.h"
%include "subscriberstatetable.h"

%apply std::string& OUTPUT {std::string &op};
%apply std::string& OUTPUT {std::string &data};
%apply std::vector<std::pair<std::string, std::string>>& OUTPUT {std::vector<std::pair<std::string, std::string>> &values};
%include "notificationconsumer.h"
%clear std::string &op;
%clear std::string &data;
%clear std::vector<std::pair<std::string, std::string>> &values;

%include "notificationproducer.h"
%include "warm_restart.h"
