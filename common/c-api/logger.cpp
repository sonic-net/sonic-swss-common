#include <string>

#include "../logger.h"
#include "logger.h"
#include "util.h"

using namespace swss;
using namespace std;

SWSSResult SWSSLogger_linkToDbWithOutput(const char *dbName, SWSSPriorityChangeNotify prioChangeNotify, const char *defLogLevel, SWSSOutputChangeNotify outputChangeNotify, const char *defOutput) {
    std::string logLevelStr(defLogLevel);
    std::string outputStr(defOutput);
    
    SWSSTry(Logger::linkToDbWithOutput(string(dbName), 
        [prioChangeNotify](std::string component, std::string newPriority) {
                prioChangeNotify(component.c_str(), newPriority.c_str());  // Convert std::string to const char*
        },
        logLevelStr, 
        [outputChangeNotify](std::string component, std::string newOutput) {
                outputChangeNotify(component.c_str(), newOutput.c_str());  // Convert std::string to const char*
        },
        outputStr));
}

SWSSResult SWSSLogger_restartLogger() {
    SWSSTry(Logger::restartLogger());
}

