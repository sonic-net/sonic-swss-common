#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <limits>

#include "common/json.h"
#include "common/json.hpp"

using namespace std;

namespace swss {

string JSon::buildJson(const vector<FieldValueTuple> &fv)
{
    nlohmann::json j = nlohmann::json::array();

    // we use array to save order
    for (const auto &i : fv)
    {
        j.push_back(fvField(i));
        j.push_back(fvValue(i));
    }

    return j.dump();
}

void JSon::readJson(const string &jsonstr, vector<FieldValueTuple> &fv)
{
    nlohmann::json j = nlohmann::json::parse(jsonstr);

    FieldValueTuple e;

    for (size_t i = 0; i < j.size(); i+=2)
    {
        fvField(e) = j[i];
        fvValue(e) = j[i+1];
        fv.push_back(e);
    }
}

bool JSon::loadJsonFromFile(ifstream &fs, vector<KeyOpFieldsValuesTuple> &db_items)
{
    nlohmann::json json_array;

    try
    {
        fs >> json_array;
    }
    catch (const std::logic_error &e)
    {
        SWSS_LOG_ERROR("Unable to parse json from the input stream: %s", e.what());
        return false;
    }
    catch (const std::bad_alloc &e)
    {
        SWSS_LOG_ERROR("Unable to parse json from the input stream: %s", e.what());
        return false;
    }

    if (!json_array.is_array())
    {
        SWSS_LOG_ERROR("Root element must be an array.");
        return false;
    }

    for (size_t i = 0; i < json_array.size(); i++)
    {
        auto &arr_item = json_array[i];

        if (arr_item.is_object())
        {
            if (el_count != arr_item.size())
            {
                SWSS_LOG_ERROR("Child elements must have both key and op entry. %s",
                               arr_item.dump().c_str());
                return false;
            }

            db_items.emplace_back();
            auto &cur_db_item = db_items.back();

            for (auto child_it = arr_item.begin(); child_it != arr_item.end(); child_it++)
            {
                auto cur_obj_key = child_it.key();
                auto &cur_obj = child_it.value();

                if (cur_obj.is_object())
                {
                    kfvKey(cur_db_item) = cur_obj_key;
                    for (nlohmann::json::iterator cur_obj_it = cur_obj.begin(); cur_obj_it != cur_obj.end(); cur_obj_it++)
                    {
                        string field_str = cur_obj_it.key();
                        string value_str;
                        if ((*cur_obj_it).is_number())
                            value_str = to_string((*cur_obj_it).get<int>());
                        else if ((*cur_obj_it).is_string())
                            value_str = (*cur_obj_it).get<string>();
                        kfvFieldsValues(cur_db_item).emplace_back(field_str, value_str);
                    }
                }
                else
                {
                    auto op = cur_obj.get<string>();

                    if (op != "SET" && op != "DEL")
                    {
                        SWSS_LOG_ERROR("Child elements' op field must be SET or DEL, but got %s, ignored", op.c_str());
                        db_items.pop_back();
                        break;
                    }
                    kfvOp(cur_db_item) = op;
                }
            }
        }
        else
        {
            SWSS_LOG_ERROR("Child elements must be objects. element:%s", arr_item.dump().c_str());
            return false;
        }
    }
    return true;
}

}
