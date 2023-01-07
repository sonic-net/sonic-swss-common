#ifndef __LOGGER_UT_H__
#define __LOGGER_UT_H__

void setLoglevel(swss::DBConnector& db, const  std::string& key, const  std::string& loglevel);

void clearConfigDB();

void prioNotify(const  std::string &component, const  std::string &prioStr);

void checkLoglevel(swss::DBConnector& db, const std::string& key, const  std::string& loglevel);


#endif /* __LOGGER_UT_H__ */
