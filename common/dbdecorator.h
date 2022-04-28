#pragma once

#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class redisReply;

namespace swss {

// Decorator only designed for swss-common developer, not for user of swss-common.
// Suggest every decorator type only have 1 decorator class.
// And in DBConnector, every decorator type can only attach 1 instance, attach new instance will return old one.
enum DBDecoratorType
{
    ReadDecorator
};

class DBDecorator
{
public:
    virtual DBDecoratorType type() = 0;
    virtual void decorate(const std::string &key, std::vector<std::pair<std::string, std::string> > &result) = 0;
    virtual void decorate(const std::string &key, std::map<std::string, std::string> &result) = 0;
    virtual void decorate(const std::string &key, redisReply *&ctx, std::insert_iterator<std::unordered_map<std::string, std::string> > &result) = 0;
    virtual void decorate(const std::string &key, redisReply *&ctx, std::insert_iterator<std::map<std::string, std::string> > &result) = 0;
    virtual std::shared_ptr<std::string> decorate(const std::string &key, const std::string &field) = 0;
};

class ConfigDBReadDecorator : public DBDecorator
{
public:
    static std::shared_ptr<DBDecorator> Create(std::string separator);

    DBDecoratorType type() override;
    void decorate(const std::string &key, std::vector<std::pair<std::string, std::string> > &result) override;
    void decorate(const std::string &key, std::map<std::string, std::string> &result) override;
    void decorate(const std::string &key, redisReply *&ctx, std::insert_iterator<std::unordered_map<std::string, std::string> > &result) override;
    void decorate(const std::string &key, redisReply *&ctx, std::insert_iterator<std::map<std::string, std::string> > &result) override;
    std::shared_ptr<std::string> decorate(const std::string &key, const std::string &field) override;

private:
    // Because derived class shared_ptr issue, not allow create ConfigDBReadDecorator, please use Create()
    ConfigDBReadDecorator(std::string separator);
    std::tuple<size_t, std::string, std::string> getTableAndRow(const std::string &key);
    
    // decorate will be invoke in template method, but virtual method not support template, so create internal method for common code.
    template<typename OutputIterator>
    void _decorate(const std::string &key, redisReply *&ctx, OutputIterator &result);
    
    template<typename OutputIterator>
    void _decorate(const std::string &key, OutputIterator &result);

    std::string m_separator;
};

}
