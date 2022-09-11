#include "common/dbconnector.h"
#include "common/producerstatetable.h"
#include "common/consumerstatetable.h"
#include "common/select.h"
#include "common/schema.h"
#include "common/loglevel.h"
#include "logger_ut.h"
#include "gtest/gtest.h"
#include <unistd.h>
using namespace std;
using namespace swss;



TEST(LOGLEVEL, loglevel)
{
    DBConnector db("CONFIG_DB", 0);
    clearConfigDB();

    string key1 = "table1";
    //string key2 = "table2", key3 = "SAI_table3";

    cout << "Setting log level NOTICE for table1." << endl;
    setLoglevel(db, key1, "NOTICE");
    //cout << "Setting log level DEBUG for table1." << endl;
    //setLoglevel(db, key2, "DEBUG");
    //cout << "Setting log level SAI_LOG_LEVEL_ERROR for table1." << endl;
    //setLoglevel(db, key3, "SAI_LOG_LEVEL_ERROR");

    sleep(1);


    char* argv[] = {"loglevel", "-d"};
    int argc = sizeof(argv) / sizeof(argv[0]);

    swssloglevel(argc, argv);
    //sleep(1);

    cout << "Checking log level for tables." << endl;
    //checkLoglevel(db, key1, "NOTICE");
    //checkLoglevel(db, key2, "NOTICE");
    //checkLoglevel(db, key3, "SAI_LOG_LEVEL_NOTICE");

    cout << "Done." << endl;
}
