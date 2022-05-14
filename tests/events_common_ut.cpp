#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include <deque>
#include <regex>
#include "gtest/gtest.h"
#include "common/events_common.h"

using namespace std;

const char *test_cfg_data = "{\
\"events\" : {  \
    \"xsub_path\": \"xsub_path\", \
    \"pair_path\": \"pair_path\" \
    }\
}";


TEST(events_common, get_config)
{
    EXPECT_EQ(string("tcp://127.0.0.1:5570"), get_config(string(XSUB_END_KEY)));
    EXPECT_EQ(string("tcp://127.0.0.1:5573"), get_config(string(PAIR_END_KEY)));
    EXPECT_EQ(string("5"), get_config(string(STATS_UPD_SECS)));

    ofstream tfile;
    const char *tfile_name = "/tmp/init_cfg.json";
    tfile.open (tfile_name);
    tfile << test_cfg_data << "\n";
    tfile.close();

    read_init_config(tfile_name);

    EXPECT_EQ(string(XSUB_END_KEY), get_config(string(XSUB_END_KEY)));
    EXPECT_EQ(string(PAIR_END_KEY), get_config(string(PAIR_END_KEY)));
    EXPECT_EQ(string("5"), get_config(string(STATS_UPD_SECS)));

    cout << "events_common: get_config succeeded\n";
}

TEST(events_common, ts)
{
    string s = get_timestamp();

    string reg = "^(?:[1-9]\\d{3}-(?:(?:0[1-9]|1[0-2])-(?:0[1-9]|1\\d|2[0-8])|(?:0[13-9]|1[0-2])-(?:29|30)|(?:0[13578]|1[02])-31)|(?:[1-9]\\d(?:0[48]|[2468][048]|[13579][26])|(?:[2468][048]|[13579][26])00)-02-29)T(?:[01]\\d|2[0-3]):[0-5]\\d:[0-5]\\d\\.[0-9]{1,6}Z$";

    cout << s << "\n";
    EXPECT_TRUE(regex_match (s, regex(reg)));
}

TEST(events_common, msg)
{
    map<string, string> t = {
        { "foo", "bar" },
        { "test", "5" },
        { "hello", "world" } };

    map<string, string> t1;

    zmq_msg_t msg;

    map_to_zmsg(t, msg);

    EXPECT_NE(t, t1);

    zmsg_to_map(msg, t1);

    EXPECT_EQ(t, t1);
}

