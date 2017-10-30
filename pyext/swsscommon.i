%module swsscommon

%include <std_string.i>
%include <std_vector.i>
%include <std_pair.i>
%template(FieldValuePair) std::pair<std::string, std::string>;
%template(FieldValuePairs) std::vector<std::pair<std::string, std::string>>;
%apply std::string& OUTPUT {std::string &key};
%apply std::string& OUTPUT {std::string &op};

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
#include "subscriberstatetable.h"
#include "notificationconsumer.h"
%}

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
%include "consumertablebase.h"
%include "consumerstatetable.h"
%include "subscriberstatetable.h"
%include "notificationconsumer.h"
