#include "portmap.h"

using namespace std;

namespace swss {

map<set<int>, string> handlePortMap(string file)
{
    map<set<int>, string> port_map;

    ifstream infile(file);
    if (!infile.is_open())
        throw "Cannot open port map configuration file!";

    string line;
    while (getline(infile, line))
    {
        if (line.at(0) == '#')
            continue;

        istringstream iss_line(line);
        string alias, lanes, lane;
        set<int> lane_set;

        iss_line >> alias >> lanes;
        istringstream iss_lane(lanes);

        while (getline(iss_lane, lane, ','))
            lane_set.insert(stoi(lane));

        port_map[lane_set] = alias;
    }

    infile.close();

    return port_map;
}

}
