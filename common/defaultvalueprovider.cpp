#include <boost/algorithm/string.hpp>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <dirent.h> 
#include <stdio.h> 

#include <libyang/libyang.h>

#include "defaultvalueprovider.h"
#include "logger.h"

using namespace std;
using namespace swss;

#ifdef DEBUG
#include <execinfo.h>
#include <signal.h>
#define TRACE_STACK_SIZE 30
[[noreturn]] void sigv_handler(int sig) {
  void *stack_info[TRACE_STACK_SIZE];
  size_t stack_info_size;

  // get void*'s for all entries on the stack
  stack_info_size = backtrace(stack_info, TRACE_STACK_SIZE);

  // print out all the frames to stderr
  cerr << "Error: signal " << sig << ":\n" >> endl;
  backtrace_symbols_fd(array, (int)size, STDERR_FILENO);
  exit(1);
}
#endif

[[noreturn]] void ThrowRunTimeError(string message)
{
    SWSS_LOG_ERROR("DefaultValueProvider: %s", message.c_str());
    throw std::runtime_error(message);
}

TableInfoBase::TableInfoBase()
{
    // C++ need this empty ctor
}

std::shared_ptr<std::string> TableInfoBase::GetDefaultValue(std::string row, std::string field)
{
    assert(!string::empty(row));
    assert(!string::empty(field));

    SWSS_LOG_DEBUG("TableInfoBase::GetDefaultValue %s %s\n", row.c_str(), field.c_str());
    FieldDefaultValueMapping *field_mapping_ptr;
    if (!FindFieldMappingByKey(row, &field_mapping_ptr)) {
        SWSS_LOG_DEBUG("Can't found default value mapping for row %s\n", row.c_str());
        return nullptr;
    }

    auto field_data = field_mapping_ptr->find(field);
    if (field_data == field_mapping_ptr->end())
    {
        SWSS_LOG_DEBUG("Can't found default value for field %s\n", field.c_str());
        return nullptr;
    }

    SWSS_LOG_DEBUG("Found default value for field %s=%s\n", field.c_str(), field_data->second.c_str());
    return std::make_shared<std::string>(field_data->second);
}

// existed_values and target_values can be same container.
void TableInfoBase::AppendDefaultValues(string row, std::map<std::string, std::string>& existed_values, map<string, string>& target_values)
{
    assert(!string::empty(row));

    SWSS_LOG_DEBUG("TableInfoBase::AppendDefaultValues %s\n", row.c_str());
    FieldDefaultValueMapping *field_mapping_ptr;
    if (!FindFieldMappingByKey(row, &field_mapping_ptr)) {
        SWSS_LOG_DEBUG("Can't found default value mapping for row %s\n", row.c_str());
        return;
    }

    for (auto &default_value : *field_mapping_ptr)
    {
        auto field_data = existed_values.find(default_value.first);
        if (field_data != existed_values.end())
        {
            // ignore when a field already has value in existed_values
            continue;
        }

        SWSS_LOG_DEBUG("Append default value: %s=%s\n",default_value.first.c_str(), default_value.second.c_str());
        target_values.emplace(default_value.first, default_value.second);
    }
}

TableInfoDict::TableInfoDict(KeyInfoToDefaultValueInfoMapping &field_info_mapping)
{
    for (auto& field_mapping : field_info_mapping)
    {
        // KeyInfo.first is key value
        string key_value = field_mapping.first.first;
        m_default_value_mapping.emplace(key_value, field_mapping.second);
    }
}

bool TableInfoDict::FindFieldMappingByKey(string row, FieldDefaultValueMapping ** founded_mapping_ptr)
{
    assert(!string::empty(row));
    assert(founded_mapping_ptr != nullptr);

    SWSS_LOG_DEBUG("TableInfoDict::FindFieldMappingByKey %s\n", row.c_str());
    auto key_result = m_default_value_mapping.find(row);
    *founded_mapping_ptr = key_result->second.get();
    return key_result == m_default_value_mapping.end();
}

TableInfoSingleList::TableInfoSingleList(KeyInfoToDefaultValueInfoMapping &field_info_mapping)
{
    m_default_value_mapping = field_info_mapping.begin()->second;
}

bool TableInfoSingleList::FindFieldMappingByKey(string row, FieldDefaultValueMapping ** founded_mapping_ptr)
{
    assert(!string::empty(row));
    assert(founded_mapping_ptr != nullptr);

    SWSS_LOG_DEBUG("TableInfoSingleList::FindFieldMappingByKey %s\n", row.c_str());
    *founded_mapping_ptr = m_default_value_mapping.get();
    return true;
}

TableInfoMultipleList::TableInfoMultipleList(KeyInfoToDefaultValueInfoMapping &field_info_mapping)
{
    for (auto& field_mapping : field_info_mapping)
    {
        // KeyInfo.second is field count
        int field_count = field_mapping.first.second;
        m_default_value_mapping.emplace(field_count, field_mapping.second);
    }
}

bool TableInfoMultipleList::FindFieldMappingByKey(string row, FieldDefaultValueMapping ** founded_mapping_ptr)
{
    assert(!string::empty(row));
    assert(founded_mapping_ptr != nullptr);

    SWSS_LOG_DEBUG("TableInfoMultipleList::FindFieldMappingByKey %s\n", row.c_str());
    int field_count = (int)std::count(row.begin(), row.end(), '|') + 1;
    auto key_info = m_default_value_mapping.find(field_count);
    
    // when not found, key_info still a valied iterator
    *founded_mapping_ptr = key_info->second.get();
    
    // return false when not found
    return key_info != m_default_value_mapping.end();
}

DefaultValueProvider& DefaultValueProvider::Instance()
{
    static DefaultValueProvider instance;
    if (instance.context == nullptr)
    {
        instance.Initialize();
    }

    return instance;
}

shared_ptr<TableInfoBase> DefaultValueProvider::FindDefaultValueInfo(std::string table)
{
    assert(!string::empty(table));

    SWSS_LOG_DEBUG("DefaultValueProvider::FindDefaultValueInfo %s\n", table.c_str());
    auto find_result = m_default_value_mapping.find(table);
    if (find_result == m_default_value_mapping.end())
    {
        SWSS_LOG_DEBUG("Not found default value info for %s\n", table.c_str());
        return nullptr;
    }
    
    return find_result->second;
}

std::shared_ptr<std::string> DefaultValueProvider::GetDefaultValue(std::string table, std::string row, std::string field)
{
    assert(!string::empty(table));
    assert(!string::empty(row));
    assert(!string::empty(field));

    SWSS_LOG_DEBUG("DefaultValueProvider::GetDefaultValue %s %s %s\n", table.c_str(), row.c_str(), field.c_str());
#ifdef DEBUG
    if (!FeatureEnabledByEnvironmentVariable())
    {
        return nullptr;
    }
#endif

    auto default_value_info = FindDefaultValueInfo(table);
    if (default_value_info == nullptr)
    {
        SWSS_LOG_DEBUG("Not found default value info for %s\n", table.c_str());
        return nullptr;
    }

    return default_value_info->GetDefaultValue(row, field);
}

void DefaultValueProvider::AppendDefaultValues(std::string table, std::string row, std::vector<std::pair<std::string, std::string> > &values)
{
    assert(!string::empty(table));
    assert(!string::empty(row));

    SWSS_LOG_DEBUG("DefaultValueProvider::AppendDefaultValues %s %s\n", table.c_str(), row.c_str());
#ifdef DEBUG
    if (!FeatureEnabledByEnvironmentVariable())
    {
        return;
    }
#endif

    auto default_value_info = FindDefaultValueInfo(table);
    if (default_value_info == nullptr)
    {
        SWSS_LOG_DEBUG("Not found default value info for %s\n", table.c_str());
        return;
    }
    
    map<string, string> existed_values;
    map<string, string> default_values;
    for (auto& field_value_pair : values)
    {
        existed_values.emplace(field_value_pair.first, field_value_pair.second);
    }

    default_value_info->AppendDefaultValues(row, existed_values, default_values);

    for (auto& field_value_pair : default_values)
    {
        values.emplace_back(field_value_pair.first, field_value_pair.second);
    }
}

void DefaultValueProvider::AppendDefaultValues(string table, string row, map<string, string>& values)
{
    assert(!string::empty(table));
    assert(!string::empty(row));

    SWSS_LOG_DEBUG("DefaultValueProvider::AppendDefaultValues %s %s\n", table.c_str(), row.c_str());
#ifdef DEBUG
    if (!FeatureEnabledByEnvironmentVariable())
    {
        return;
    }
#endif

    auto default_value_info = FindDefaultValueInfo(table);
    if (default_value_info == nullptr)
    {
        SWSS_LOG_DEBUG("Not found default value info for %s\n", table.c_str());
        return;
    }

    default_value_info->AppendDefaultValues(row, values, values);
}

DefaultValueProvider::DefaultValueProvider()
{
#ifdef DEBUG
    // initialize crash event handler for debug.
    signal(SIGSEGV, sigv_handler);
#endif
}


DefaultValueProvider::~DefaultValueProvider()
{
    if (context)
    {
        // set private_destructor to NULL because no any private data
        ly_ctx_destroy(context, NULL);
    }
}

void DefaultValueProvider::Initialize(char* module_path)
{
    assert(module_path != nullptr && !string::empty(module_path));
    assert(context == nullptr);

    DIR *module_dir = opendir(module_path);
    if (!module_dir)
    {
        ThrowRunTimeError("Open Yang model path " + string(module_path) + " failed");
    }

    context = ly_ctx_new(module_path, LY_CTX_ALLIMPLEMENTED);
    struct dirent *sub_dir;
    while ((sub_dir = readdir(module_dir)) != NULL)
    {
        if (sub_dir->d_type == DT_REG)
        {
            SWSS_LOG_DEBUG("file_name: %s\n", sub_dir->d_name);
            string file_name(sub_dir->d_name);
            int pos = (int)file_name.find(".yang");
            string module_name = file_name.substr(0, pos);

            const struct lys_module *module = ly_ctx_load_module(
                                                    context,
                                                    module_name.c_str(),
                                                    EMPTY_STR); // Use EMPTY_STR to revision to load the latest revision
            if (module->data == NULL)
            {
                // Not every yang file should contains yang model
                SWSS_LOG_WARN("Yang file %s does not contains model %s.\n", sub_dir->d_name, module_name.c_str());
                continue;
            }

            struct lys_node *top_level_node = module->data;
            while (top_level_node)
            {
                if (top_level_node->nodetype != LYS_CONTAINER)
                {
                    SWSS_LOG_DEBUG("ignore top level element %s, tyoe %d\n",top_level_node->name, top_level_node->nodetype);
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
    assert(table_child_node != nullptr);
    SWSS_LOG_DEBUG("DefaultValueProvider::GetKeyInfo %s\n",table_child_node->name);

    int key_field_count = 0;
    string key_value = "";
    if (table_child_node->nodetype == LYS_LIST)
    {
        SWSS_LOG_DEBUG("Child list: %s\n",table_child_node->name);

        // when a top level container contains list, the key defined by the 'keys' field.
        struct lys_node_list *list_node = (struct lys_node_list*)table_child_node;
        string key(list_node->keys_str);
        key_field_count = (int)std::count(key.begin(), key.end(), '|') + 1;
    }
    else if (table_child_node->nodetype == LYS_CONTAINER)
    {
        SWSS_LOG_DEBUG("Child container name: %s\n",table_child_node->name);

        // when a top level container not contains any list, the key is child container name
        key_value = string(table_child_node->name);
    }
    else
    {
        SWSS_LOG_DEBUG("Ignore child element: %s\n",table_child_node->name);
        return nullptr;
    }
    
    return make_shared<KeyInfo>(key_value, key_field_count);
}

FieldDefaultValueMappingPtr DefaultValueProvider::GetDefaultValueInfo(struct lys_node* table_child_node)
{
    assert(table_child_node != nullptr);
    SWSS_LOG_DEBUG("DefaultValueProvider::GetDefaultValueInfo %s\n",table_child_node->name);

    auto field = table_child_node->child;
    auto field_mapping = make_shared<FieldDefaultValueMapping>();
    while (field)
    {
        if (field->nodetype == LYS_LEAF)
        {
            SWSS_LOG_DEBUG("leaf field: %s\n",field->name);
            struct lys_node_leaf *leaf_node = (struct lys_node_leaf*)field;
            if (leaf_node->dflt)
            {
                SWSS_LOG_DEBUG("field: %s, default: %s\n",leaf_node->name, leaf_node->dflt);
                (*field_mapping)[string(leaf_node->name)] = string(leaf_node->dflt);
            }
        }
        else if (field->nodetype == LYS_CHOICE)
        {
            SWSS_LOG_DEBUG("choice field: %s\n",field->name);
            struct lys_node_choice *choice_node = (struct lys_node_choice *)field;
            if (choice_node->dflt)
            {
                // TODO: convert choice default value to string
                SWSS_LOG_DEBUG("choice field: %s, default: TBD\n",choice_node->name);
            }
        }
        else if (field->nodetype == LYS_LEAFLIST)
        {
            SWSS_LOG_DEBUG("list field: %s\n",field->name);
            struct lys_node_leaflist *list_node = (struct lys_node_leaflist *)field;
            if (list_node->dflt)
            {
                // TODO: convert list default value to json string
                SWSS_LOG_DEBUG("list field: %s, default: TBD\n",list_node->name);
            }
        }
#ifdef DEBUG
        else
        {
            SWSS_LOG_DEBUG("Field %s with type %d does not support default value\n",field->name, field->nodetype);
        }
#endif

        field = field->next;
    }

    return field_mapping;
}

int DefaultValueProvider::BuildFieldMappingList(struct lys_node* table, KeyInfoToDefaultValueInfoMapping &field_info_mapping)
{
    assert(table != nullptr);

    int child_list_count = 0;
    auto next_child = table->child;
    while (next_child)
    {
        // get key from schema
        auto keyInfo = GetKeyInfo(next_child);
        if (keyInfo == nullptr)
        {
            next_child = next_child->next;
            continue;
        }
        else if (keyInfo->second != 0)
        {
            // when key field count not 0, it's a list node.
            child_list_count++;
        }

        // get field name to default value mappings from schema
        field_info_mapping.emplace(*keyInfo, GetDefaultValueInfo(next_child));

        next_child = next_child->next;
    }

    return child_list_count;
}

// Load default value info from yang model and append to default value mapping
void DefaultValueProvider::AppendTableInfoToMapping(struct lys_node* table)
{
    assert(table != nullptr);

    SWSS_LOG_DEBUG("DefaultValueProvider::AppendTableInfoToMapping table name: %s\n",table->name);
    KeyInfoToDefaultValueInfoMapping field_info_mapping;
    int list_count = BuildFieldMappingList(table, field_info_mapping);

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
    m_default_value_mapping.emplace(table_name, shared_ptr<TableInfoBase>(table_info_ptr));
}

#ifdef DEBUG
bool DefaultValueProvider::FeatureEnabledByEnvironmentVariable()
{
    const char* show_default = getenv("CONFIG_DB_DEFAULT_VALUE");
    if (show_default == nullptr || strcmp(show_default, "TRUE") != 0)
    {
        // enable feature with "export CONFIG_DB_DEFAULT_VALUE=TRUE"
        SWSS_LOG_DEBUG("enable feature with \"export CONFIG_DB_DEFAULT_VALUE=TRUE\"\n");
        return false;
    }

    return true;
}
#endif