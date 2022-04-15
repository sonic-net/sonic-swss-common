#ifndef __DEFAULTVALUEPROVIDER__
#define __DEFAULTVALUEPROVIDER__
#include <map>
#include <string>

#define DEFAULT_YANG_MODULE_PATH "/usr/local/yang-models"
#define EMPTY_STR ""

struct ly_ctx;

// Key information
typedef std::pair<std::string, int> KeyInfo;

// Field name to default value mapping
typedef std::map<std::string, std::string> FieldDefaultValueMapping;
typedef std::shared_ptr<FieldDefaultValueMapping> FieldDefaultValueMappingPtr;

// Key info to default value info mapping
typedef std::map<KeyInfo, FieldDefaultValueMappingPtr> KeyInfoToDefaultValueInfoMapping;

namespace swss {

class TableInfoBase
{
public:
    TableInfoBase();

    void AppendDefaultValues(std::string row, std::map<std::string, std::string>& source_values, std::map<std::string, std::string>& target_values);

    std::shared_ptr<std::string> GetDefaultValue(std::string row, std::string field);

protected:
    virtual bool FindFieldMappingByKey(std::string row, FieldDefaultValueMapping ** founded_mapping_ptr) = 0;
};

class TableInfoDict : public TableInfoBase
{
public:
    TableInfoDict(KeyInfoToDefaultValueInfoMapping &field_info_mapping);

private:
    // Mapping: key value -> field -> default 
    std::map<std::string, FieldDefaultValueMappingPtr> m_default_value_mapping;

    bool FindFieldMappingByKey(std::string row, FieldDefaultValueMapping ** founded_mapping_ptr);
};

class TableInfoSingleList : public TableInfoBase
{
public:
    TableInfoSingleList(KeyInfoToDefaultValueInfoMapping &field_info_mapping);

private:
    // Mapping: field -> default 
    FieldDefaultValueMappingPtr m_default_value_mapping;

    bool FindFieldMappingByKey(std::string row, FieldDefaultValueMapping ** founded_mapping_ptr);
};

struct TableInfoMultipleList : public TableInfoBase
{
public:
    TableInfoMultipleList(KeyInfoToDefaultValueInfoMapping &field_info_mapping);

private:
    // Mapping: key field count -> field -> default 
    std::map<int, FieldDefaultValueMappingPtr> m_default_value_mapping;

    bool FindFieldMappingByKey(std::string row, std::map<std::string, std::string> ** founded_mapping_ptr);
};

class DefaultValueProvider
{
public:
    static DefaultValueProvider& Instance();

    void AppendDefaultValues(std::string table, std::string row, std::map<std::string, std::string>& values);

    void AppendDefaultValues(std::string table, std::string row, std::vector<std::pair<std::string, std::string> > &values);

    std::shared_ptr<std::string> GetDefaultValue(std::string table, std::string row, std::string field);

private:
    DefaultValueProvider();
    ~DefaultValueProvider();

    //  libyang context
    struct ly_ctx *context = nullptr;

    // The table name to table default value info mapping
    std::map<std::string, std::shared_ptr<TableInfoBase> > m_default_value_mapping;

    void Initialize(char* module_path = DEFAULT_YANG_MODULE_PATH);

    // Load default value info from yang model and append to default value mapping
    void AppendTableInfoToMapping(struct lys_node* table);

    std::shared_ptr<TableInfoBase> FindDefaultValueInfo(std::string table);

    int BuildFieldMappingList(struct lys_node* table, KeyInfoToDefaultValueInfoMapping& field_mapping_list);
    
    std::shared_ptr<KeyInfo> GetKeyInfo(struct lys_node* table_child_node);
    FieldDefaultValueMappingPtr GetDefaultValueInfo(struct lys_node* table_child_node);
};

}
#endif