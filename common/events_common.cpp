#include "events_common.h"
#include <boost/serialization/map.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

/*
 * defaults for all config entries
 */
#define CFG_VAL map_str_str_t::value_type
map_str_str_t cfg_data = {
    CFG_VAL(XSUB_END_KEY, "tcp://127.0.0.1:5570"),
    CFG_VAL(XPUB_END_KEY, "tcp://127.0.0.1:5571"),
    CFG_VAL(REQ_REP_END_KEY, "tcp://127.0.0.1:5572"),
    CFG_VAL(PAIR_END_KEY, "tcp://127.0.0.1:5573"),
    CFG_VAL(STATS_UPD_SECS, 5)
};

void
_read_init_config()
{
    ifstream fs (INIT_CFG_PATH);

    if (!fs.is_open())
        return;

    stringstream buffer;
    buffer << fs.rdbuf();

    const auto &data = nlohmann::json::parse(buffer.str());

    const auto it = data.find(EVENTS_KEY);
    if (it == data.end())
        return;

    const auto edata = *it;
    for (events_json_data_t::iterator itJ = cfg_data.begin();
            itJ != cfg_data.end(); ++itJ) {
        auto itE = edata.find(itJ->first);
        if (itE != edata.end()) {
            itJ->second = *itE;
        }
    }

    return;
}

string
get_config(const string key)
{
    static bool init = false;

    if (!init) {
        _read_init_config();
        init = true;
    }   
    /* Intentionally crash for non-existing key, as this
     * is internal code bug
     */
    return cfg_data[key];
}

const string
get_timestamp()
{
    std::stringstream ss, sfrac;

    auto timepoint = system_clock::now();
    std::time_t tt = system_clock::to_time_t (timepoint);
    struct std::tm * ptm = std::localtime(&tt);

    uint64_t ms = duration_cast<microseconds>(timepoint.time_since_epoch()).count();
    uint64_t sec = duration_cast<seconds>(timepoint.time_since_epoch()).count();
    uint64_t mfrac = ms - (sec * 1000 * 1000);

    sfrac << mfrac;

    ss << put_time(ptm, "%b %e %H:%M:%S.") << sfrac.str().substr(0, 6) << "Z";
    return ss.str();
}


/*
 * Way to serialize map
 * boost::archive::text_oarchive could be used to archive any struct/class
 * but that class needs some additional support, that declares
 * boost::serialization::access as private friend and couple more tweaks
 * std::map inherently supports serialization
 */
const string
serialize(const map_str_str_t& data)
{
    std::stringstream ss;
    boost::archive::text_oarchive oarch(ss);
    oarch << data;
    return ss.str();
}

void
deserialize(const string& s, map_str_str_t& data)
{
    std::stringstream ss;
    ss << s;
    boost::archive::text_iarchive iarch(ss);
    iarch >> data;
    return;
}


void
map_to_zmsg(const map_str_str_t& data, zmq_msg_t &msg)
{
    string s = serialize(data);

    zmq_msg_init_size(&msg, s.size());
    strncpy((char *)zmq_msg_data(&msg), s.c_str(), s.size());
}


void
zmsg_to_map(zmq_msg_t &msg, map_str_str_t& data)
{
    string s((const char *)zmq_msg_data(&msg), zmq_msg_size(&msg));
    deserialize(s, data);
}

