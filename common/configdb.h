#pragma once

#include <string>
#include <map>
#include "sonicv2connector.h"
#include "redistran.h"

namespace swss {

class ConfigDBConnector_Native : public SonicV2Connector_Native
{
public:
    static constexpr const char *INIT_INDICATOR = "CONFIG_DB_INITIALIZED";

    ConfigDBConnector_Native(bool use_unix_socket_path = false, const char *netns = "");

    void db_connect(std::string db_name, bool wait_for_init = false, bool retry_on = false);
    void connect(bool wait_for_init = true, bool retry_on = false);

    void set_entry(std::string table, std::string key, const std::map<std::string, std::string>& data);
    void mod_entry(std::string table, std::string key, const std::map<std::string, std::string>& data);
    std::map<std::string, std::string> get_entry(std::string table, std::string key);
    std::vector<std::string> get_keys(std::string table, bool split = true);
    std::map<std::string, std::map<std::string, std::string>> get_table(std::string table);
    void delete_table(std::string table);
    virtual void mod_config(const std::map<std::string, std::map<std::string, std::map<std::string, std::string>>>& data);
    virtual std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> get_config();

    std::string getKeySeparator() const;
    std::string getTableNameSeparator() const;
    std::string getDbName() const;

protected:
    std::string m_table_name_separator = "|";
    std::string m_key_separator = "|";

    std::string m_db_name;
};

#ifdef SWIG
%pythoncode %{
    ## Note: diamond inheritance, reusing functions in both classes
    class ConfigDBConnector(SonicV2Connector, ConfigDBConnector_Native):

        ## Note: there is no easy way for SWIG to map ctor parameter netns(C++) to namespace(python)
        def __init__(self, use_unix_socket_path = False, namespace = '', **kwargs):
            if 'decode_responses' in kwargs and kwargs.pop('decode_responses') != True:
                raise ValueError('decode_responses must be True if specified, False is not supported')
            if namespace is None:
                namespace = ''
            super(ConfigDBConnector, self).__init__(use_unix_socket_path = use_unix_socket_path, namespace = namespace)

            # Trick: to achieve static/instance method "overload", we must use initize the function in ctor
            # ref: https://stackoverflow.com/a/28766809/2514803
            self.serialize_key = self._serialize_key
            self.deserialize_key = self._deserialize_key

            ## Note: callback is difficult to implement by SWIG C++, so keep in python
            self.handlers = {}

        @property
        def KEY_SEPARATOR(self):
            return self.getKeySeparator()

        @property
        def TABLE_NAME_SEPARATOR(self):
            return self.getTableNameSeparator()

        @property
        def db_name(self):
            return self.getDbName()

        ## Note: callback is difficult to implement by SWIG C++, so keep in python
        def listen(self):
            ## Start listen Redis keyspace events and will trigger corresponding handlers when content of a table changes.
            self.pubsub = self.get_redis_client(self.db_name).pubsub()
            self.pubsub.psubscribe("__keyspace@{}__:*".format(self.get_dbid(self.db_name)))
            while True:
                item = self.pubsub.listen_message()
                if item['type'] == 'pmessage':
                    key = item['channel'].split(':', 1)[1]
                    try:
                        (table, row) = key.split(self.TABLE_NAME_SEPARATOR, 1)
                        if table in self.handlers:
                            client = self.get_redis_client(self.db_name)
                            data = self.raw_to_typed(client.hgetall(key))
                            self.__fire(table, row, data)
                    except ValueError:
                        pass    #Ignore non table-formated redis entries

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

        # Note: we could not use a class variable for KEY_SEPARATOR, but original dependent code is using
        # these static functions. So we implement both static and instance functions with the same name.
        # The static function will behave according to ConfigDB separators.
        @staticmethod
        def serialize_key(key, separator='|'):
            if type(key) is tuple:
                return separator.join(key)
            else:
                return str(key)

        def _serialize_key(self, key):
            return ConfigDBConnector.serialize_key(key, self.KEY_SEPARATOR)

        @staticmethod
        def deserialize_key(key, separator='|'):
            tokens = key.split(separator)
            if len(tokens) > 1:
                return tuple(tokens)
            else:
                return key

        def _deserialize_key(self, key):
            return ConfigDBConnector.deserialize_key(key, self.KEY_SEPARATOR)

        def __fire(self, table, key, data):
            if table in self.handlers:
                handler = self.handlers[table]
                handler(table, key, data)

        def subscribe(self, table, handler):
            self.handlers[table] = handler

        def unsubscribe(self, table):
            if table in self.handlers:
                self.handlers.pop(table)

        def set_entry(self, table, key, data):
            key = self.serialize_key(key)
            raw_data = self.typed_to_raw(data)
            super(ConfigDBConnector, self).set_entry(table, key, raw_data)

        def mod_config(self, data):
            raw_config = {}
            for table_name, table_data in data.items():
                raw_config[table_name] = {}
                if table_data == None:
                    continue
                for key, data in table_data.items():
                    raw_key = self.serialize_key(key)
                    raw_data = self.typed_to_raw(data)
                    raw_config[table_name][raw_key] = raw_data
            super(ConfigDBConnector, self).mod_config(raw_config)

        def mod_entry(self, table, key, data):
            key = self.serialize_key(key)
            raw_data = self.typed_to_raw(data)
            super(ConfigDBConnector, self).mod_entry(table, key, raw_data)

        def get_entry(self, table, key):
            key = self.serialize_key(key)
            raw_data = super(ConfigDBConnector, self).get_entry(table, key)
            return self.raw_to_typed(raw_data)

        def get_keys(self, table, split=True):
            keys = super(ConfigDBConnector, self).get_keys(table, split)
            ret = []
            for key in keys:
                ret.append(self.deserialize_key(key))
            return ret

        def get_table(self, table):
            data = super(ConfigDBConnector, self).get_table(table)
            ret = {}
            for row, entry in data.items():
                entry = self.raw_to_typed(entry)
                ret[self.deserialize_key(row)] = entry
            return ret

        def get_config(self):
            data = super(ConfigDBConnector, self).get_config()
            ret = {}
            for table_name, table in data.items():
                for row, entry in table.items():
                    entry = self.raw_to_typed(entry)
                    ret.setdefault(table_name, {})[self.deserialize_key(row)] = entry
            return ret
%}
#endif

class ConfigDBPipeConnector_Native: public ConfigDBConnector_Native
{
public:
    ConfigDBPipeConnector_Native(bool use_unix_socket_path = false, const char *netns = "");

    void mod_config(const std::map<std::string, std::map<std::string, std::map<std::string, std::string>>>& data) override;
    std::map<std::string, std::map<std::string, std::map<std::string, std::string>>> get_config() override;

private:
    static const int64_t REDIS_SCAN_BATCH_SIZE = 30;

    int _delete_entries(DBConnector& client, RedisTransactioner& pipe, const char *pattern, int cursor);
    void _delete_table(DBConnector& client, RedisTransactioner& pipe, std::string table);
    void _mod_entry(RedisTransactioner& pipe, std::string table, std::string key, const std::map<std::string, std::string>& data);
    int _get_config(DBConnector& client, RedisTransactioner& pipe, std::map<std::string, std::map<std::string, std::map<std::string, std::string>>>& data, int cursor);
};

#ifdef SWIG
%pythoncode %{
    class ConfigDBPipeConnector(ConfigDBConnector, ConfigDBPipeConnector_Native):

        ## Note: diamond inheritance, reusing functions in both classes
        def __init__(self, **kwargs):
            super(ConfigDBPipeConnector, self).__init__(**kwargs)
%}
#endif
}
