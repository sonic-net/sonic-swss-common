#ifndef __JSON__
#define __JSON__

#include <string>
#include <fstream>
#include <vector>

#include "table.h"

namespace swss {

class JSon
{
private:
    /*
     * el_count represents the number of elements in each object,
     * which means each object have exactly 2 elements: the data and the operator
     */
    static const int el_count = 2;
public:
    static std::string buildJson(const std::vector<FieldValueTuple> &fv);
    static void readJson(const std::string &json, std::vector<FieldValueTuple> &fv);
    /*
       bool loadJsonFromFile(std::ifstream &fs, std::vector<KeyOpFieldsValuesTuple> &db_items);

       parse the json file and construct a vector of KeyOpFieldsValuesTuple as the result
       the json file should be a list objects with each consisting of a data field and an operator field
         - the data should be a dictionary
         - the operator field should be a string, "SET" or "DEL"
       an example:
       [
           {
               "QOS_TABLE:TC_TO_QUEUE_MAP_TABLE:AZURE": {
                   "5": "1",
                   "6": "1"
               },
               "OP": "SET"
           },
           {
               "QOS_TABLE:DSCP_TO_TC_MAP_TABLE:AZURE": {
                   "7":"5",
                   "6":"5",
                   "3":"3",
                   "8":"7",
                   "9":"8"
               },
               "OP": "SET"
           }
       ]
      
       parameters:
         fs: the input ifstream representing the json file
         fv: the output vector
       return: boolean
         True: the input json file has been successfully parsed
         False: there are some errors found
     */
    static bool loadJsonFromFile(std::ifstream &fs, std::vector<KeyOpFieldsValuesTuple> &db_items);
};

}

#endif
