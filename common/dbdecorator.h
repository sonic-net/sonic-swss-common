#pragma once

#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class redisReply;

namespace swss {

class DBDecorator
{
public:
    virtual void decorate(const std::string &key, std::vector<std::pair<std::string, std::string> > &result) = 0;
    virtual void decorate(const std::string &key, std::map<std::string, std::string> &result) = 0;
    virtual void decorate(const std::string &key, redisReply *&ctx, std::insert_iterator<std::unordered_map<std::string, std::string> > &result) = 0;
    virtual void decorate(const std::string &key, redisReply *&ctx, std::insert_iterator<std::map<std::string, std::string> > &result) = 0;
    virtual std::shared_ptr<std::string> decorate(const std::string &key, const std::string &field) = 0;
};

class ConfigDBDecorator : public DBDecorator
{
public:
    static std::shared_ptr<DBDecorator> Create(std::string separator);

    void decorate(const std::string &key, std::vector<std::pair<std::string, std::string> > &result) override;
    void decorate(const std::string &key, std::map<std::string, std::string> &result) override;
    void decorate(const std::string &key, redisReply *&ctx, std::insert_iterator<std::unordered_map<std::string, std::string> > &result) override;
    void decorate(const std::string &key, redisReply *&ctx, std::insert_iterator<std::map<std::string, std::string> > &result) override;
    std::shared_ptr<std::string> decorate(const std::string &key, const std::string &field) override;

private:
    // Because derived class shared_ptr issue, not allow create ConfigDBDecorator, please use Create()
    ConfigDBDecorator(std::string separator);
    std::tuple<size_t, std::string, std::string> getTableAndRow(const std::string &key);
    
    // decorate will be invoke in template method, but virtual method not support template, so create internal method for common code.
    template<typename OutputIterator>
    void decorateInternal(const std::string &key, redisReply *&ctx, OutputIterator &result);
    
    template<typename OutputIterator>
    void decorateInternal(const std::string &key, OutputIterator &result);

    std::string m_separator;
};

}
