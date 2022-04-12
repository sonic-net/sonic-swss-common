#ifndef __DEFAULTVALUEPROVIDER__
#define __DEFAULTVALUEPROVIDER__

#define DEFAULT_YANG_MODULE_PATH "/usr/local/yang-models"
#define EMPTY_STR ""


// Key information
typedef std::tuple<std::string, unsigned int> KeyInfo;

// Key information
typedef std::map<std::string, std::string> DefaultValueInfo;

// Key info to default value info mapping
typedef std::map<KeyInfo, std::shared_ptr<DefaultValueInfo> KeyInfoToDefaultValueInfoMapping;

namespace swss {

class TableInfoBase;
struct ly_ctx;

class TableInfoBase
{
public:
    TableInfoBase();

    bool TryAppendDefaultValues(std::string key, std::map<std::string, std::string>& target_values);

protected:
    virtual bool FindFieldMappingByKey(std::string key, std::map<std::string, std::string> ** founded_mapping_ptr) = 0;
};

class TableInfoDict : public TableInfoBase
{
public:
    TableInfoDict(KeyInfoToDefaultValueInfoMapping &field_info_mapping);

private:
    // Mapping: key value -> field -> default 
    std::map<std::string, std::map<std::string, std::string> > defaultValueMapping;

    bool FindFieldMappingByKey(std::string key, std::map<std::string, std::string> ** founded_mapping_ptr);
};

class TableInfoSingleList : public TableInfoBase
{
public:
    TableInfoSingleList(KeyInfoToDefaultValueInfoMapping &field_info_mapping);

private:
    // Mapping: field -> default 
    std::map<std::string, std::string> defaultValueMapping;

    bool FindFieldMappingByKey(std::string key, std::map<std::string, std::string> ** founded_mapping_ptr);
};

struct TableInfoMultipleList : public TableInfoBase
{
public:
    TableInfoMultipleList(KeyInfoToDefaultValueInfoMapping &field_info_mapping);

private:
    // Mapping: key field count -> field -> default 
    std::map<unsigned int, std::map<std::string, std::string> > defaultValueMapping;

    bool FindFieldMappingByKey(std::string key, std::map<std::string, std::string> ** founded_mapping_ptr);
};

class DefaultValueProvider
{
public:
    DefaultValueProvider& Instance();

    void AppendDefaultValues(std::string table, std::map<std::string, std::map<std::string, std::string> >& values);

    void AppendDefaultValues(std::string table, std::string key, std::map<std::string, std::string>& values);

private:
    DefaultValueProvider() {};
    ~DefaultValueProvider();

    //  libyang context
    struct ly_ctx *context = NULL;

    // The table name to table default value info mapping
    std::map<std::string, std::shared_ptr<TableInfoBase> > default_value_mapping;

    void Initialize(char* module_path = DEFAULT_YANG_MODULE_PATH);

    // Load default value info from yang model and append to default value mapping
    void AppendTableInfoToMapping(struct lys_node* table);

    std::shared_ptr<TableInfoBase> FindDefaultValueInfo(std::string table);

    unsigned int BuildFieldMappingList(struct lys_node* table, KeyInfoToDefaultValueInfoMapping& field_mapping_list);
    
    std::shared_ptr<KeyInfo> GetKeyInfo(struct lys_node* table_child_node);
    std::shared_ptr<DefaultValueInfo> GetDefaultValueInfo(struct lys_node* table_child_node);
}

}
#endif