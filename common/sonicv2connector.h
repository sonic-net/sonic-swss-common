#pragma once

#include <stdint.h>
#include <unistd.h>

#include "dbinterface.h"

namespace swss
{

class SonicV2Connector
{
public:
    SonicV2Connector(bool use_unix_socket_path = false, const char *netns = "");

    std::string getNamespace() const;

#ifdef SWIG
    %pythoncode %{
        __swig_getmethods__["namespace"] = getNamespace
        __swig_setmethods__["namespace"] = None
        if _newclass: namespace = property(getNamespace, None)
    %}
#endif

    void connect(const std::string& db_name, bool retry_on = true);

    void close(const std::string& db_name);

    std::vector<std::string> get_db_list();

    int get_dbid(const std::string& db_name);

    std::string get_db_separator(const std::string& db_name);

    DBConnector& get_redis_client(const std::string& db_name);

    int64_t publish(const std::string& db_name, const std::string& channel, const std::string& message);

    bool exists(const std::string& db_name, const std::string& key);

    std::vector<std::string> keys(const std::string& db_name, const char *pattern="*", bool blocking=false);

    std::string get(const std::string& db_name, const std::string& _hash, const std::string& key, bool blocking=false);

    std::map<std::string, std::string> get_all(const std::string& db_name, const std::string& _hash, bool blocking=false);

    int64_t set(const std::string& db_name, const std::string& _hash, const std::string& key, const std::string& val, bool blocking=false);

    int64_t del(const std::string& db_name, const std::string& key, bool blocking=false);

    void delete_all_by_pattern(const std::string& db_name, const std::string& pattern);

private:
    std::string get_db_socket(const std::string& db_name);

    std::string get_db_hostname(const std::string& db_name);

    int get_db_port(const std::string& db_name);

    DBInterface m_dbintf;
    bool m_use_unix_socket_path;
    std::string m_netns;
};

#ifdef SWIG
// TRICK!
// Note: there is no easy way for SWIG to map ctor parameter netns(C++) to namespace(python),
// so we use python patch to achieve this
// TODO: implement it with formal SWIG syntax, which will be target language independent
%pythoncode %{
    _old_SonicV2Connector__init__ = SonicV2Connector.__init__
    def _new_SonicV2Connector__init__(self, use_unix_socket_path = False, namespace = ''):
        if namespace is None:
            namespace = ''
        _old_SonicV2Connector__init__(self, use_unix_socket_path = use_unix_socket_path, netns = namespace)

        # Add database name attributes into SonicV2Connector instance
        # Note: this is difficult to implement in C++
        for db_name in self.get_db_list():
            # set a database name as a constant value attribute.
            setattr(self, db_name, db_name)
            getmethod = lambda self: db_name
            SonicV2Connector.__swig_getmethods__[db_name] = getmethod
            SonicV2Connector.__swig_setmethods__[db_name] = None

    SonicV2Connector.__init__ = _new_SonicV2Connector__init__
%}
#endif
}
