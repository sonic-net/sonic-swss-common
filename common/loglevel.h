/*
void usage(const std::string &program, int status, const std::string &message);

void setLoglevel(swss::Table& logger_tbl, const std::string& component, const std::string& loglevel);

bool validateSaiLoglevel(const std::string &prioStr);

bool filterOutSaiKeys(const std::string& key);

bool filterSaiKeys(const std::string& key);

std::vector<std::string> get_sai_keys(std::vector<std::string> keys);

std::vector<std::string> get_no_sai_keys(std::vector<std::string> keys);

void setAllLoglevel(swss::Table& logger_tbl, std::vector<std::string> components,std::string loglevel);
*/
int swssloglevel(int argc, char **argv);
