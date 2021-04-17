%module swsscommon

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

#include "schema.h"
#include "dbconnector.h"
#include "dbinterface.h"
#include "sonicv2connector.h"
#include "pubsub.h"
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
#include "logger.h"
#include "configdb.h"
%}

%include <std_string.i>
%include <std_vector.i>
%include <std_pair.i>
%include <std_map.i>
%include <typemaps.i>
%include <stdint.i>
%include <exception.i>

%template(FieldValuePair) std::pair<std::string, std::string>;
%template(FieldValuePairs) std::vector<std::pair<std::string, std::string>>;
%template(FieldValueMap) std::map<std::string, std::string>;
%template(VectorString) std::vector<std::string>;
%template(ScanResult) std::pair<int64_t, std::vector<std::string>>;
%template(GetTableResult) std::map<std::string, std::map<std::string, std::string>>;
%template(GetConfigResult) std::map<std::string, std::map<std::string, std::map<std::string, std::string>>>;

%exception {
    try
    {
        class PyThreadStateGuard
        {
            PyThreadState *m_save;
        public:
            PyThreadStateGuard()
            {
                m_save = PyEval_SaveThread();
            }
            ~PyThreadStateGuard()
            {
                PyEval_RestoreThread(m_save);
            }
        } thread_state_guard;

        $action
    }
    SWIG_CATCH_STDEXCEPT // catch std::exception derivatives
    catch (...)
    {
        SWIG_exception(SWIG_UnknownError, "unknown exception");
    }
}

%template(FieldValuePair) std::pair<std::string, std::string>;
%template(FieldValuePairs) std::vector<std::pair<std::string, std::string>>;
%template(FieldValuePairsList) std::vector<std::vector<std::pair<std::string, std::string>>>;
%template(FieldValueMap) std::map<std::string, std::string>;
%template(VectorString) std::vector<std::string>;
%template(ScanResult) std::pair<int64_t, std::vector<std::string>>;
%template(GetTableResult) std::map<std::string, std::map<std::string, std::string>>;
%template(GetConfigResult) std::map<std::string, std::map<std::string, std::map<std::string, std::string>>>;

%pythoncode %{
    def _FieldValueMap__get(self, key, default=None):
        if key in self:
            return self[key]
        else:
            return default

    def _FieldValueMap__update(self, *args, **kwargs):
        other = dict(*args, **kwargs)
        for key in other:
            self[key] = other[key]

    FieldValueMap.get = _FieldValueMap__get
    FieldValueMap.update = _FieldValueMap__update
%}

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
    temp = SWIG_NewPointerObj(*$1, SWIGTYPE_p_swss__Selectable, 0);
    SWIG_Python_AppendOutput($result, temp);
}

%inline %{
template <typename T>
T castSelectableObj(swss::Selectable *temp)
{
    return dynamic_cast<T>(temp);
}
%}

%template(CastSelectableToRedisSelectObj) castSelectableObj<swss::RedisSelect *>;
%template(CastSelectableToSubscriberTableObj) castSelectableObj<swss::SubscriberStateTable *>;

%include "schema.h"
%include "dbconnector.h"
%include "sonicv2connector.h"
%include "pubsub.h"
%include "selectable.h"
%include "select.h"
%include "rediscommand.h"
%include "redispipeline.h"
%include "redisselect.h"
%include "redistran.h"
%include "configdb.h"

%extend swss::DBConnector {
    %template(hgetall) hgetall<std::map<std::string, std::string>>;
}

%ignore swss::TableEntryPoppable::pops(std::deque<KeyOpFieldsValuesTuple> &, const std::string &);
%apply std::vector<std::string>& OUTPUT {std::vector<std::string> &keys};
%apply std::vector<std::string>& OUTPUT {std::vector<std::string> &ops};
%apply std::vector<std::vector<std::pair<std::string, std::string>>>& OUTPUT {std::vector<std::vector<std::pair<std::string, std::string>>> &fvss};
%apply std::vector<std::pair<std::string, std::string>>& OUTPUT {std::vector<std::pair<std::string, std::string>> &ovalues};
%apply std::string& OUTPUT {std::string &value};
%include "table.h"
%clear std::vector<std::string> &keys;
%clear std::vector<std::string> &ops;
%clear std::vector<std::vector<std::pair<std::string, std::string>>> &fvss;
%clear std::vector<std::pair<std::string, std::string>> &values;
%clear std::string &value;

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
%include "dbinterface.h"
%include "logger.h"
