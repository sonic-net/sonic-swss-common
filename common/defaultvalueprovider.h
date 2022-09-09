#pragma once

#include <map>
#include <string>
#include <vector>
#include "common/table.h"

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

    void AppendDefaultValues(const std::string &row, std::map<std::string, std::string>& sourceValues, std::map<std::string, std::string>& targetValues);

    std::shared_ptr<std::string> GetDefaultValue(const std::string &row, const std::string &field);

protected:
    virtual bool FindFieldMappingByKey(const std::string &row, FieldDefaultValueMapping ** foundedMappingPtr) = 0;
};

class TableInfoDict : public TableInfoBase
{
public:
    TableInfoDict(KeyInfoToDefaultValueInfoMapping &fieldInfoMapping);

private:
    // Mapping: key value -> field -> default 
    std::map<std::string, FieldDefaultValueMappingPtr> m_defaultValueMapping;

    bool FindFieldMappingByKey(const std::string &row, FieldDefaultValueMapping ** foundedMappingPtr);
};

class TableInfoSingleList : public TableInfoBase
{
public:
    TableInfoSingleList(KeyInfoToDefaultValueInfoMapping &fieldInfoMapping);

private:
    // Mapping: field -> default 
    FieldDefaultValueMappingPtr m_defaultValueMapping;

    bool FindFieldMappingByKey(const std::string &row, FieldDefaultValueMapping ** foundedMappingPtr);
};

struct TableInfoMultipleList : public TableInfoBase
{
public:
    TableInfoMultipleList(KeyInfoToDefaultValueInfoMapping &fieldInfoMapping);

private:
    // Mapping: key field count -> field -> default 
    std::map<int, FieldDefaultValueMappingPtr> m_defaultValueMapping;

    bool FindFieldMappingByKey(const std::string &row, FieldDefaultValueMapping ** foundedMappingPtr);
};

class DefaultValueProvider
{
public:
    static DefaultValueProvider& instance();

    void appendDefaultValues(const std::string &table, const std::string &key, std::vector<std::pair<std::string, std::string>> &values);

    std::shared_ptr<std::string> getDefaultValue(const std::string &table, const std::string &key, const std::string &field);

    std::map<std::string, std::string> getDefaultValues(const std::string &table, const std::string &key);

#ifndef UNITTEST
private:
#endif
    DefaultValueProvider();
    ~DefaultValueProvider();

    //  libyang context
    struct ly_ctx *m_context = nullptr;

    // The table name to table default value info mapping
    std::map<std::string, std::shared_ptr<TableInfoBase> > m_defaultValueMapping;

    void Initialize(char* modulePath = DEFAULT_YANG_MODULE_PATH);
    
    void LoadModule(const std::string name, const std::string path, struct ly_ctx *context);

    // Load default value info from yang model and append to default value mapping
    void AppendTableInfoToMapping(struct lys_node* table);

    std::shared_ptr<TableInfoBase> FindDefaultValueInfo(const std::string &table);

    int BuildFieldMappingList(struct lys_node* table, KeyInfoToDefaultValueInfoMapping& fieldMappingList);
    
    std::shared_ptr<KeyInfo> GetKeyInfo(struct lys_node* table_child_node);
    FieldDefaultValueMappingPtr GetDefaultValueInfo(struct lys_node* tableChildNode);

    void InternalAppendDefaultValues(const std::string &table, const std::string &key, std::map<std::string, std::string>& existedValues, std::map<std::string, std::string>& defaultValues);

    void GetDefaultValueInfoForChoice(struct lys_node* field, std::shared_ptr<FieldDefaultValueMapping> fieldMapping);
    void GetDefaultValueInfoForLeaf(struct lys_node* field, std::shared_ptr<FieldDefaultValueMapping> fieldMapping);
    void GetDefaultValueInfoForLeaflist(struct lys_node* field, std::shared_ptr<FieldDefaultValueMapping> fieldMapping);
};

}
