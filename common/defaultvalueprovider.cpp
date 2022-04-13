#include <boost/algorithm/string.hpp>
#include <map>
#include <vector>
#include <iostream>
#include <string>

#include <dirent.h> 
#include <stdio.h> 

#include <libyang/libyang.h>

#include "defaultvalueprovider.h"
#include "logger.h"

using namespace std;
using namespace swss;

void ThrowRunTimeError(string message)
{
    SWSS_LOG_ERROR(message);
    throw std::runtime_error(message);
}

TableInfoBase::TableInfoBase()
{
    // C++ need this empty ctor
}

bool TableInfoBase::TryAppendDefaultValues(string key, map<string, string>& target_values)
{
    FieldDefaultValueMapping *field_mapping_ptr;
    if (!FindFieldMappingByKey(key, &field_mapping_ptr)) {
        return false;
    }

    for (auto &default_value : *field_mapping_ptr)
    {
        auto field_data = target_values.find(default_value.first);
        if (field_data != target_values.end())
        {
            // ignore when a field already has value in config DB
            continue;
        }

        target_values[default_value.first] = default_value.second;
    }

    return true;
}

TableInfoDict::TableInfoDict(KeyInfoToDefaultValueInfoMapping &field_info_mapping)
{
    for (auto& field_mapping : field_info_mapping)
    {
        string fieldName = std::get<0>(field_mapping.first);
        this->defaultValueMapping[fieldName] = field_mapping.second;
    }
}

bool TableInfoDict::FindFieldMappingByKey(string key, FieldDefaultValueMapping ** founded_mapping_ptr)
{
    auto key_result = this->defaultValueMapping.find(key);
    *founded_mapping_ptr = key_result->second.get();
    return key_result == this->defaultValueMapping.end();
}

TableInfoSingleList::TableInfoSingleList(KeyInfoToDefaultValueInfoMapping &field_info_mapping)
{
    this->defaultValueMapping = field_info_mapping.begin()->second;
}

bool TableInfoSingleList::FindFieldMappingByKey(string key, FieldDefaultValueMapping ** founded_mapping_ptr)
{
    *founded_mapping_ptr = this->defaultValueMapping.get();
    return true;
}

TableInfoMultipleList::TableInfoMultipleList(KeyInfoToDefaultValueInfoMapping &field_info_mapping)
{
    for (auto& field_mapping : field_info_mapping)
    {
        unsigned int fieldCount = std::get<1>(field_mapping.first);
        this->defaultValueMapping[fieldCount] = field_mapping.second;
    }
}

bool TableInfoMultipleList::FindFieldMappingByKey(string key, FieldDefaultValueMapping ** founded_mapping_ptr)
{
    unsigned int key_field_count = std::count(key.begin(), key.end(), '|') + 1;
    auto key_result = this->defaultValueMapping.find(key_field_count);
    *founded_mapping_ptr = key_result->second.get();
    return key_result == this->defaultValueMapping.end();
}

DefaultValueProvider& DefaultValueProvider::Instance()
{
    static DefaultValueProvider instance;
    return instance;
}

shared_ptr<TableInfoBase> DefaultValueProvider::FindDefaultValueInfo(std::string table)
{
    auto find_result = this->default_value_mapping.find(table);
    if (find_result == this->default_value_mapping.end())
    {
        SWSS_LOG_WARN("Not found default value info for table: %s\n", table.c_str());
        return nullptr;
    }
    
    return find_result->second;
}

void DefaultValueProvider::AppendDefaultValues(string table, map<string, map<string, string> >& values)
{
    if (this->context == nullptr)
    {
        this->Initialize();
    }

    auto default_value_info = this->FindDefaultValueInfo(table);
    if (default_value_info == nullptr)
    {
        return;
    }

    for (auto& row : values)
    {
        default_value_info->TryAppendDefaultValues(row.first, row.second);
    }
}

void DefaultValueProvider::AppendDefaultValues(string table, string key, map<string, string>& values)
{
    if (this->context == nullptr)
    {
        this->Initialize();
    }

    auto default_value_info = this->FindDefaultValueInfo(table);
    if (default_value_info == nullptr)
    {
        return;
    }

    default_value_info->TryAppendDefaultValues(key, values);
}

DefaultValueProvider::~DefaultValueProvider()
{
    if (this->context)
    {
        // set private_destructor to NULL because no any private data
        ly_ctx_destroy(this->context, NULL);
    }
}

void DefaultValueProvider::Initialize(char* module_path)
{
    if (this->context)
    {
        ThrowRunTimeError("DefaultValueProvider already initialized");
    }
    
    this->context = ly_ctx_new(module_path, LY_CTX_ALLIMPLEMENTED);
    DIR *module_dir = opendir(module_path);
    if (!module_dir)
    {
        ThrowRunTimeError("Open Yang model path " + string(module_path) + " failed");
    }

    struct dirent *sub_dir;
    while ((sub_dir = readdir(module_dir)) != NULL)
    {
        if (sub_dir->d_type == DT_REG)
        {
            SWSS_LOG_DEBUG("file_name: %s\n", sub_dir->d_name);
            string file_name(sub_dir->d_name);
            unsigned int pos = file_name.find(".yang");
            string module_name = file_name.substr(0, pos);

            const struct lys_module *module = ly_ctx_load_module(
                                                    this->context,
                                                    module_name.c_str(),
                                                    EMPTY_STR); // Use EMPTY_STR to revision to load the latest revision
            if (module->data == NULL)
            {
                // Every yang file should contains yang model
                SWSS_LOG_WARN("Yang file " + file_name + " does not contains any model.\n");
                continue;
            }

            struct lys_node *top_level_node = module->data;
            while (top_level_node)
            {
                if (top_level_node->nodetype != LYS_CONTAINER)
                {
                    // Config DB table schema is defined by top level container
                    top_level_node = top_level_node->next;
                    continue;
                }

                SWSS_LOG_DEBUG("top level container: %s\n",top_level_node->name);
                auto container = top_level_node->child;
                while (container)
                {
                    SWSS_LOG_DEBUG("container name: %s\n",container->name);

                    AppendTableInfoToMapping(container);
                    container = container->next;
                }

                top_level_node = top_level_node->next;
            }
        }
    }
    closedir(module_dir);
}

std::shared_ptr<KeyInfo> DefaultValueProvider::GetKeyInfo(struct lys_node* table_child_node)
{
    unsigned int key_field_count = 0;
    string key_value = "";
    if (table_child_node->nodetype == LYS_LIST)
    {
        SWSS_LOG_DEBUG("child list: %s\n",table_child_node->name);

        // when a top level container contains list, the key defined by the 'keys' field.
        struct lys_node_list *list_node = (struct lys_node_list*)table_child_node;
        string key(list_node->keys_str);
        key_field_count = std::count(key.begin(), key.end(), '|') + 1;
    }
    else if (table_child_node->nodetype == LYS_CONTAINER)
    {
        SWSS_LOG_DEBUG("child container name: %s\n",table_child_node->name);

        // when a top level container not contains any list, the key is child container name
        key_value = string(table_child_node->name);
    }
    else
    {
        return nullptr;
    }
    
    return make_shared<KeyInfo>(key_value, key_field_count);
}

FieldDefaultValueMappingPtr DefaultValueProvider::GetDefaultValueInfo(struct lys_node* table_child_node)
{
    auto field = table_child_node->child;
    auto field_mapping = make_shared<FieldDefaultValueMapping>();
    while (field)
    {
        if (field->nodetype == LYS_LEAF)
        {
            struct lys_node_leaf *leaf_node = (struct lys_node_leaf*)field;
            if (leaf_node->dflt)
            {
                SWSS_LOG_DEBUG("field: %s, default: %s\n",leaf_node->name, leaf_node->dflt);
                (*field_mapping)[string(leaf_node->name)] = string(leaf_node->dflt);
            }
        }
        else if (field->nodetype == LYS_CHOICE)
        {
            struct lys_node_choice *choice_node = (struct lys_node_choice *)field;
            if (choice_node->dflt)
            {
                // TODO: convert choice default value to string
                SWSS_LOG_DEBUG("choice field: %s, default: TBD\n",choice_node->name);
            }
        }
        else if (field->nodetype == LYS_LEAFLIST)
        {
            struct lys_node_leaflist *list_node = (struct lys_node_leaflist *)field;
            if (list_node->dflt)
            {
                // TODO: convert list default value to json string
                SWSS_LOG_DEBUG("list field: %s, default: TBD\n",list_node->name);
            }
        }

        field = field->next;
    }

    return field_mapping;
}

unsigned int DefaultValueProvider::BuildFieldMappingList(struct lys_node* table, KeyInfoToDefaultValueInfoMapping &field_info_mapping)
{
    unsigned child_list_count = 0;

    auto next_child = table->child;
    while (next_child)
    {
        // get key from schema
        auto keyInfo = this->GetKeyInfo(next_child);
        if (keyInfo == nullptr)
        {
            next_child = next_child->next;
            continue;
        }
        else if (std::get<1>(*keyInfo) != 0)
        {
            // when key field count not 0, it's a list node.
            child_list_count++;
        }

        // get field name to default value mappings from schema
        field_info_mapping[*keyInfo] = this->GetDefaultValueInfo(next_child);

        next_child = next_child->next;
    }

    return child_list_count;
}

// Load default value info from yang model and append to default value mapping
void DefaultValueProvider::AppendTableInfoToMapping(struct lys_node* table)
{
    SWSS_LOG_DEBUG("table name: %s\n",table->name);
    KeyInfoToDefaultValueInfoMapping field_info_mapping;
    unsigned list_count = this->BuildFieldMappingList(table, field_info_mapping);

    // create container data by list count
    TableInfoBase* table_info_ptr = nullptr;
    if (list_count == 0)
    {
        table_info_ptr = new TableInfoDict(field_info_mapping);
    }
    else if (list_count == 1)
    {
        table_info_ptr = new TableInfoSingleList(field_info_mapping);
    }
    else
    {
        table_info_ptr = new TableInfoMultipleList(field_info_mapping);
    }

    string table_name(table->name);
    default_value_mapping[table_name] = shared_ptr<TableInfoBase>(table_info_ptr);
}