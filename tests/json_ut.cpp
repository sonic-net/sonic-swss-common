#include "common/json.hpp"
#include "common/producertable.h"
#include "gtest/gtest.h"

using namespace std;
using namespace swss;
using json = nlohmann::json;

#define TEST_VIEW       (7)
#define TEST_DUMP_FILE  "ut_dump_file.txt"

TEST(JSON, test)
{
    /* Construct the file */
    DBConnector db(TEST_VIEW, "localhost", 6379, 0);
    ProducerTable *p;
    p = new ProducerTable(&db, "UT_REDIS", TEST_DUMP_FILE);

    vector<FieldValueTuple> fvTuples;
    FieldValueTuple fv1("test_field_1", "test_value_1");
    fvTuples.push_back(fv1);

    p->set("test_key_1", fvTuples);

    FieldValueTuple fv2("test_field_2", "test_value_2");
    fvTuples.push_back(fv2);

    p->set("test_key_2", fvTuples);

    p->del("test_key_1");

    delete(p);

    /* Read the file and validate the content */
    fstream file(TEST_DUMP_FILE, fstream::in);
    json j;
    file >> j;

    EXPECT_TRUE(j.is_array());
    EXPECT_TRUE(j.size() == 3);

    for (size_t i = 0; i < j.size(); i++)
    {
        auto item = j[i];
        EXPECT_TRUE(item.is_object());
        EXPECT_TRUE(item.size() == 2);

        for (auto it = item.begin(); it != item.end(); it++)
        {
            if (it.key() == "OP")
                EXPECT_TRUE(it.value() == "SET" || it.value() == "DEL");
            else
            {
                EXPECT_TRUE(it.key() == "UT_REDIS:test_key_1" || it.key() == "UT_REDIS:test_key_2");
                auto subitem = it.value();
                EXPECT_TRUE(subitem.is_object());
                if (subitem.size() > 0)
                {
                    for (auto subit = subitem.begin(); subit != subitem.end(); subit++)
                    {
                        if (subit.key() == "test_field_1")
                            EXPECT_EQ(subit.value(), "test_value_1");
                        if (subit.key() == "test_field_2")
                            EXPECT_EQ(subit.value(), "test_value_2");
                    }
                }
            }
        }
    }
    file.close();
}
