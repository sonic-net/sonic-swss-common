#pragma once

#include <string>
#include <map>
#include "sonicv2connector.h"

namespace swss {

class ConfigDBConnector : public SonicV2Connector
{
public:
    ConfigDBConnector(bool use_unix_socket_path = false, const char *netns = "");

    void db_connect(std::string db_name, bool wait_for_init, bool retry_on);
    void connect(bool wait_for_init = true, bool retry_on = false);

    void set_entry(std::string table, std::string key, const std::unordered_map<std::string, std::string>& data);
    void mod_entry(std::string table, std::string key, const std::unordered_map<std::string, std::string>& data);
    std::unordered_map<std::string, std::string> get_entry(std::string table, std::string key);
    std::vector<std::string> get_keys(std::string table, bool split = true);
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> get_table(std::string table);
    void delete_table(std::string table);
    void mod_config(const std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>>& data);
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> get_config();

    std::string getKeySeparator() const;

#ifdef SWIG
    %pythoncode %{
        __swig_getmethods__["KEY_SEPARATOR"] = getKeySeparator
        __swig_setmethods__["KEY_SEPARATOR"] = None
        if _newclass: KEY_SEPARATOR = property(getKeySeparator, None)

        ## Dynamic typed functions used in python
        @staticmethod
        def raw_to_typed(raw_data):
            if raw_data is None:
                return None
            typed_data = {}
            for raw_key in raw_data:
                key = raw_key

                # "NULL:NULL" is used as a placeholder for objects with no attributes
                if key == "NULL":
                    pass
                # A column key with ending '@' is used to mark list-typed table items
                # TODO: Replace this with a schema-based typing mechanism.
                elif key.endswith("@"):
                    value = raw_data[raw_key].split(',')
                    typed_data[key[:-1]] = value
                else:
                    typed_data[key] = raw_data[raw_key]
            return typed_data

        @staticmethod
        def typed_to_raw(typed_data):
            if typed_data is None:
                return {}
            elif len(typed_data) == 0:
                return { "NULL": "NULL" }
            raw_data = {}
            for key in typed_data:
                value = typed_data[key]
                if type(value) is list:
                    raw_data[key+'@'] = ','.join(value)
                else:
                    raw_data[key] = str(value)
            return raw_data

        @staticmethod
        def serialize_key(key):
            if type(key) is tuple:
                return ConfigDBConnector.KEY_SEPARATOR.join(key)
            else:
                return str(key)

        @staticmethod
        def deserialize_key(key):
            tokens = key.split(ConfigDBConnector.KEY_SEPARATOR)
            if len(tokens) > 1:
                return tuple(tokens)
            else:
                return key
    %}
#endif

protected:
    static constexpr const char *INIT_INDICATOR = "CONFIG_DB_INITIALIZED";
    std::string TABLE_NAME_SEPARATOR = "|";
    std::string KEY_SEPARATOR = "|";

    std::string m_db_name;
};

#ifdef SWIG
%pythoncode %{
    // TRICK!
    // Note: there is no easy way for SWIG to map ctor parameter netns(C++) to namespace(python),
    // so we use python patch to achieve this
    // TODO: implement it with formal SWIG syntax, which will be target language independent
    _old_ConfigDBConnector__init__ = ConfigDBConnector.__init__
    def _new_ConfigDBConnector__init__(self, use_unix_socket_path = False, namespace = '', **kwargs):
        if 'decode_responses' in kwargs and kwargs.pop('decode_responses') != True:
            raise ValueError('decode_responses must be True if specified, False is not supported')
        if namespace is None:
            namespace = ''
        _old_ConfigDBConnector__init__(self, use_unix_socket_path = use_unix_socket_path, netns = namespace)
    ConfigDBConnector.__init__ = _new_ConfigDBConnector__init__

    _old_ConfigDBConnector_set_entry = ConfigDBConnector.set_entry
    def _new_ConfigDBConnector_set_entry(self, table, key, data):
        key = self.serialize_key(key)
        raw_data = self.typed_to_raw(data)
        _old_ConfigDBConnector_set_entry(self, table, key, raw_data)
    ConfigDBConnector.set_entry = _new_ConfigDBConnector_set_entry

    _old_ConfigDBConnector_mod_entry = ConfigDBConnector.mod_entry
    def _new_ConfigDBConnector_mod_entry(self, table, key, data):
        key = self.serialize_key(key)
        raw_data = self.typed_to_raw(data)
        _old_ConfigDBConnector_mod_entry(self, table, key, raw_data)
    ConfigDBConnector.mod_entry = _new_ConfigDBConnector_mod_entry

    _old_ConfigDBConnector_get_entry = ConfigDBConnector.get_entry
    def _new_ConfigDBConnector_get_entry(self, table, key):
        key = self.serialize_key(key)
        raw_data = _old_ConfigDBConnector_get_entry(self, table, key)
        return self.raw_to_typed(raw_data)
    ConfigDBConnector.get_entry = _new_ConfigDBConnector_get_entry

%}
#endif
}
