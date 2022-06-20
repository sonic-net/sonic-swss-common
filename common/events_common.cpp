#include "events_common.h"

int running_ut = 0;

/*
 * defaults for all config entries
 */
#define CFG_VAL map_str_str_t::value_type
const map_str_str_t cfg_default = {
    CFG_VAL(XSUB_END_KEY, "tcp://127.0.0.1:5570"),
    CFG_VAL(XPUB_END_KEY, "tcp://127.0.0.1:5571"),
    CFG_VAL(REQ_REP_END_KEY, "tcp://127.0.0.1:5572"),
    CFG_VAL(CAPTURE_END_KEY, "tcp://127.0.0.1:5573"),
    CFG_VAL(STATS_UPD_SECS, "5"),
    CFG_VAL(CACHE_MAX_CNT, "")
};

map_str_str_t cfg_data;

sequence_t str_to_seq(const string s)
{
    stringstream ss(s);
    sequence_t seq;

    ss >> seq;

    return seq;
}

string seq_to_str(sequence_t seq)
{
    stringstream ss;
    ss << seq;
    return ss.str();
}


void
read_init_config(const char *init_cfg_file)
{
    /* Set default and override from file */
    cfg_data = cfg_default;

    if (init_cfg_file == NULL) {
        return;
    }

    ifstream fs (init_cfg_file);

    if (!fs.is_open()) 
        return;

    stringstream buffer;
    buffer << fs.rdbuf();

    const auto &data = nlohmann::json::parse(buffer.str());

    const auto it = data.find(CFG_EVENTS_KEY);
    if (it == data.end())
        return;

    const auto edata = *it;
    for (map_str_str_t::iterator itJ = cfg_data.begin();
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
    if (cfg_data.empty()) {
        read_init_config(INIT_CFG_PATH);
    }   
    /* Intentionally crash for non-existing key, as this
     * is internal code bug
     */
    return cfg_data[key];
}

const string
get_timestamp()
{
    stringstream ss, sfrac;

    auto timepoint = system_clock::now();
    time_t tt = system_clock::to_time_t (timepoint);
    struct tm * ptm = localtime(&tt);

    uint64_t ms = duration_cast<microseconds>(timepoint.time_since_epoch()).count();
    uint64_t sec = duration_cast<seconds>(timepoint.time_since_epoch()).count();
    uint64_t mfrac = ms - (sec * 1000 * 1000);

    sfrac << mfrac;

    ss << put_time(ptm, "%FT%H:%M:%S.") << sfrac.str().substr(0, 6) << "Z";
    return ss.str();
}

