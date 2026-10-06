#include "redisauth.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <limits>
#include <nlohmann/json.hpp>
#include <utility>

using namespace std;

namespace
{

constexpr size_t MIN_DECODED_CREDENTIAL_LENGTH = 32;
constexpr size_t MAX_CREDENTIAL_LENGTH = 128;
constexpr size_t MAX_PROFILE_FILE_LENGTH = 64 * 1024;

class ScopedFd
{
public:
    explicit ScopedFd(int fd = -1) : m_fd(fd) {}
    ~ScopedFd()
    {
        reset();
    }

    ScopedFd(const ScopedFd&) = delete;
    ScopedFd& operator=(const ScopedFd&) = delete;

    int get() const { return m_fd; }

    void reset(int fd = -1)
    {
        if (m_fd >= 0)
        {
            close(m_fd);
        }
        m_fd = fd;
    }

    int release()
    {
        int fd = m_fd;
        m_fd = -1;
        return fd;
    }

private:
    int m_fd;
};

int base64UrlValue(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'Z')
    {
        return ch - 'A';
    }
    if (ch >= 'a' && ch <= 'z')
    {
        return ch - 'a' + 26;
    }
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0' + 52;
    }
    if (ch == '-')
    {
        return 62;
    }
    if (ch == '_')
    {
        return 63;
    }
    return -1;
}

bool isCanonicalBase64Url(const string& value)
{
    for (unsigned char ch : value)
    {
        if (base64UrlValue(ch) < 0)
        {
            return false;
        }
    }

    const size_t remainder = value.size() % 4;
    if (remainder == 1)
    {
        return false;
    }

    if (!value.empty())
    {
        const int last = base64UrlValue(value.back());
        if ((remainder == 2 && (last & 0x0f) != 0) ||
            (remainder == 3 && (last & 0x03) != 0))
        {
            return false;
        }
    }

    size_t decodedLength = (value.size() / 4) * 3;
    if (remainder == 2)
    {
        decodedLength += 1;
    }
    else if (remainder == 3)
    {
        decodedLength += 2;
    }

    return decodedLength >= MIN_DECODED_CREDENTIAL_LENGTH;
}

bool isSafeToken(const string& value)
{
    if (value.empty() || value.size() > 128)
    {
        return false;
    }

    for (unsigned char ch : value)
    {
        if (!((ch >= 'A' && ch <= 'Z') ||
              (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') ||
              ch == '-' || ch == '_' || ch == '.'))
        {
            return false;
        }
    }

    return true;
}

bool isAbsolutePath(const string& value)
{
    return !value.empty() && value.front() == '/' &&
           value.find('\0') == string::npos;
}

bool isSafeParentDirectory(const struct stat& st)
{
    if (!S_ISDIR(st.st_mode) || st.st_uid != 0)
    {
        return false;
    }

    const mode_t writableByOthers = st.st_mode & (S_IWGRP | S_IWOTH);
    if (writableByOthers == 0)
    {
        return true;
    }

    /* Root-owned sticky directories such as /tmp cannot replace a root-owned child. */
    return (st.st_mode & S_ISVTX) != 0;
}

int openSecureFile(const string& path, const char *openError)
{
    if (!isAbsolutePath(path) || path == "/" || path.back() == '/')
    {
        throw swss::RedisAuthError(openError);
    }

    ScopedFd directory(open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    struct stat st = {};
    if (directory.get() < 0 || fstat(directory.get(), &st) != 0 ||
        !isSafeParentDirectory(st))
    {
        throw swss::RedisAuthError(openError);
    }

    size_t start = 1;
    for (;;)
    {
        size_t separator = path.find('/', start);
        const bool finalComponent = separator == string::npos;
        string component = finalComponent
            ? path.substr(start)
            : path.substr(start, separator - start);

        if (component.empty() || component == "." || component == "..")
        {
            throw swss::RedisAuthError(openError);
        }

        if (finalComponent)
        {
            int fd = openat(directory.get(), component.c_str(),
                            O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
            if (fd < 0)
            {
                throw swss::RedisAuthError(openError);
            }
            return fd;
        }

        int next = openat(directory.get(), component.c_str(),
                          O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (next < 0)
        {
            throw swss::RedisAuthError(openError);
        }

        ScopedFd nextDirectory(next);
        if (fstat(nextDirectory.get(), &st) != 0 || !isSafeParentDirectory(st))
        {
            throw swss::RedisAuthError(openError);
        }

        directory.reset(nextDirectory.release());
        start = separator + 1;
    }
}

void readExactFile(int fd, size_t size, const char *readError, string& value)
{
    value.assign(size, '\0');
    size_t offset = 0;
    while (offset < value.size())
    {
        ssize_t rc = read(fd, &value[offset], value.size() - offset);
        if (rc < 0 && errno == EINTR)
        {
            continue;
        }
        if (rc <= 0)
        {
            throw swss::RedisAuthError(readError);
        }
        offset += static_cast<size_t>(rc);
    }

    char extra;
    ssize_t rc;
    do
    {
        rc = read(fd, &extra, 1);
    }
    while (rc < 0 && errno == EINTR);

    if (rc != 0)
    {
        throw swss::RedisAuthError(readError);
    }
}

bool validTcpEndpoint(const string& hostname, int port)
{
    return !hostname.empty() && hostname.size() <= 255 &&
           hostname.find('\0') == string::npos && port > 0 && port <= 65535;
}

bool readJsonInteger(const nlohmann::json& value, int64_t minimum,
                     int64_t maximum, int64_t& result)
{
    if (value.is_number_unsigned())
    {
        uint64_t number = value.get<uint64_t>();
        if (maximum < 0 || number > static_cast<uint64_t>(maximum))
        {
            return false;
        }
        result = static_cast<int64_t>(number);
        return result >= minimum;
    }

    if (value.is_number_integer())
    {
        int64_t number = value.get<int64_t>();
        if (number < minimum || number > maximum)
        {
            return false;
        }
        result = number;
        return true;
    }

    return false;
}

bool hasAccessAcl(int fd)
{
    errno = 0;
    ssize_t size = fgetxattr(fd, "system.posix_acl_access", nullptr, 0);
    if (size >= 0)
    {
        return true;
    }

    if (errno == ENODATA || errno == ENOTSUP
#if EOPNOTSUPP != ENOTSUP
        || errno == EOPNOTSUPP
#endif
    )
    {
        return false;
    }

    throw swss::RedisAuthError("Redis credential file failed validation");
}

}

namespace swss
{

constexpr const char *RedisAuthConfig::DEFAULT_CLIENT_PROFILES_FILE;

RedisAuthConfig::RedisAuthConfig() = default;

RedisAuthConfig::RedisAuthConfig(
    const string& username,
    const string& credentialFile,
    const string& domain,
    int64_t credentialGroup,
    vector<Endpoint> endpoints)
    : m_username(username)
    , m_credentialFile(credentialFile)
    , m_domain(domain)
    , m_credentialGroup(credentialGroup)
    , m_endpoints(std::move(endpoints))
{
    if (!isSafeToken(m_username))
    {
        throw invalid_argument("Invalid Redis authentication username");
    }
    if (!isAbsolutePath(m_credentialFile))
    {
        throw invalid_argument("Redis credential file must be an absolute path");
    }
    if ((!m_domain.empty() && !isSafeToken(m_domain)) ||
        m_credentialGroup < -1 ||
        m_credentialGroup > static_cast<int64_t>(numeric_limits<gid_t>::max()) ||
        m_endpoints.empty())
    {
        throw invalid_argument("Invalid Redis authentication policy");
    }

    for (const auto& endpoint : m_endpoints)
    {
        if ((endpoint.type == EndpointType::TCP &&
             !validTcpEndpoint(endpoint.address, endpoint.port)) ||
            (endpoint.type == EndpointType::UNIX_SOCKET &&
             (!isAbsolutePath(endpoint.address) || endpoint.port != 0)))
        {
            throw invalid_argument("Invalid Redis authentication endpoint");
        }
    }
}

RedisAuthConfig RedisAuthConfig::forTcp(
    const string& username,
    const string& credentialFile,
    const string& hostname,
    int port,
    int64_t credentialGroup)
{
    return RedisAuthConfig(username, credentialFile, string(), credentialGroup,
                           {{EndpointType::TCP, hostname, port}});
}

RedisAuthConfig RedisAuthConfig::forUnixSocket(
    const string& username,
    const string& credentialFile,
    const string& unixPath,
    int64_t credentialGroup)
{
    return RedisAuthConfig(username, credentialFile, string(), credentialGroup,
                           {{EndpointType::UNIX_SOCKET, unixPath, 0}});
}

RedisAuthConfig RedisAuthConfig::fromProfile(const string& profile, const string& profilesFile)
{
    if (!isSafeToken(profile) || !isAbsolutePath(profilesFile))
    {
        throw RedisAuthError("Redis authentication profile is invalid");
    }

    ScopedFd profileFd(openSecureFile(
        profilesFile, "Redis authentication profile file could not be opened"));

    struct stat st = {};
    if (fstat(profileFd.get(), &st) != 0 ||
        !S_ISREG(st.st_mode) ||
        st.st_uid != 0 ||
        st.st_gid != 0 ||
        st.st_nlink != 1 ||
        (st.st_mode & 07777) != 0444 ||
        st.st_size <= 0 ||
        st.st_size > static_cast<off_t>(MAX_PROFILE_FILE_LENGTH))
    {
        throw RedisAuthError("Redis authentication profile file failed validation");
    }

    string contents;
    readExactFile(profileFd.get(), static_cast<size_t>(st.st_size),
                  "Redis authentication profile file could not be read", contents);

    try
    {
        auto root = nlohmann::json::parse(contents);
        auto version = root.find("schema_version");
        auto profiles = root.find("profiles");
        int64_t schemaVersion = 0;
        if (!root.is_object() ||
            version == root.end() ||
            !readJsonInteger(*version, 1, 1, schemaVersion) ||
            profiles == root.end() || !profiles->is_object())
        {
            throw RedisAuthError("Redis authentication profile file failed validation");
        }

        auto entry = profiles->find(profile);
        if (entry == profiles->end() || !entry->is_object())
        {
            throw RedisAuthError("Redis authentication profile was not found");
        }

        auto username = entry->find("username");
        auto domain = entry->find("domain");
        auto credentialFile = entry->find("credential_file");
        auto endpoints = entry->find("endpoints");
        if (username == entry->end() || !username->is_string() ||
            domain == entry->end() || !domain->is_string() ||
            !isSafeToken(domain->get<string>()) ||
            credentialFile == entry->end() || !credentialFile->is_string() ||
            endpoints == entry->end() || !endpoints->is_array() || endpoints->empty())
        {
            throw RedisAuthError("Redis authentication profile file failed validation");
        }

        int64_t credentialGroup = -1;
        auto group = entry->find("credential_gid");
        if (group != entry->end())
        {
            if (!readJsonInteger(
                    *group, 0,
                    static_cast<int64_t>(numeric_limits<gid_t>::max()),
                    credentialGroup))
            {
                throw RedisAuthError("Redis authentication profile file failed validation");
            }
        }

        vector<Endpoint> allowedEndpoints;
        for (const auto& endpoint : *endpoints)
        {
            if (!endpoint.is_object())
            {
                throw RedisAuthError("Redis authentication profile file failed validation");
            }

            auto transport = endpoint.find("transport");
            if (transport == endpoint.end() || !transport->is_string())
            {
                throw RedisAuthError("Redis authentication profile file failed validation");
            }

            if (transport->get<string>() == "tcp")
            {
                auto hostname = endpoint.find("hostname");
                auto port = endpoint.find("port");
                int64_t portNumber = 0;
                if (hostname == endpoint.end() || !hostname->is_string() ||
                    port == endpoint.end() ||
                    !readJsonInteger(*port, 1, 65535, portNumber))
                {
                    throw RedisAuthError("Redis authentication profile file failed validation");
                }
                allowedEndpoints.push_back(
                    {EndpointType::TCP, hostname->get<string>(),
                     static_cast<int>(portNumber)});
            }
            else if (transport->get<string>() == "unix")
            {
                auto path = endpoint.find("path");
                if (path == endpoint.end() || !path->is_string())
                {
                    throw RedisAuthError("Redis authentication profile file failed validation");
                }
                allowedEndpoints.push_back(
                    {EndpointType::UNIX_SOCKET, path->get<string>(), 0});
            }
            else
            {
                throw RedisAuthError("Redis authentication profile file failed validation");
            }
        }

        try
        {
            return RedisAuthConfig(
                username->get<string>(), credentialFile->get<string>(),
                domain->get<string>(), credentialGroup, std::move(allowedEndpoints));
        }
        catch (const invalid_argument&)
        {
            throw RedisAuthError("Redis authentication profile file failed validation");
        }
    }
    catch (const RedisAuthError&)
    {
        throw;
    }
    catch (...)
    {
        throw RedisAuthError("Redis authentication profile file failed validation");
    }
}

bool RedisAuthConfig::isConfigured() const
{
    return !m_username.empty();
}

const string& RedisAuthConfig::getUsername() const
{
    return m_username;
}

const string& RedisAuthConfig::getCredentialFile() const
{
    return m_credentialFile;
}

const string& RedisAuthConfig::getDomain() const
{
    return m_domain;
}

bool RedisAuthConfig::operator==(const RedisAuthConfig& other) const
{
    if (m_username != other.m_username ||
        m_credentialFile != other.m_credentialFile ||
        m_domain != other.m_domain ||
        m_credentialGroup != other.m_credentialGroup ||
        m_endpoints.size() != other.m_endpoints.size())
    {
        return false;
    }

    for (size_t i = 0; i < m_endpoints.size(); ++i)
    {
        const auto& left = m_endpoints[i];
        const auto& right = other.m_endpoints[i];
        if (left.type != right.type || left.address != right.address ||
            left.port != right.port)
        {
            return false;
        }
    }

    return true;
}

void RedisAuthConfig::validateTcpEndpoint(const string& hostname, int port) const
{
    if (!isConfigured())
    {
        return;
    }

    for (const auto& endpoint : m_endpoints)
    {
        if (endpoint.type == EndpointType::TCP && endpoint.address == hostname &&
            endpoint.port == port)
        {
            return;
        }
    }

    throw RedisAuthError("Redis authentication profile does not allow target endpoint");
}

void RedisAuthConfig::validateUnixEndpoint(const string& unixPath) const
{
    if (!isConfigured())
    {
        return;
    }

    for (const auto& endpoint : m_endpoints)
    {
        if (endpoint.type == EndpointType::UNIX_SOCKET && endpoint.address == unixPath)
        {
            return;
        }
    }

    throw RedisAuthError("Redis authentication profile does not allow target endpoint");
}

void RedisAuthConfig::readCredential(string& credential) const
{
    if (!isConfigured())
    {
        throw RedisAuthError("Redis authentication is not configured");
    }

    ScopedFd credentialFd(openSecureFile(
        m_credentialFile, "Redis credential file could not be opened"));

    struct stat st = {};
    if (fstat(credentialFd.get(), &st) != 0 ||
        !S_ISREG(st.st_mode) ||
        st.st_uid != 0 ||
        st.st_nlink != 1 ||
        hasAccessAcl(credentialFd.get()) ||
        !(((st.st_mode & 07777) == 0400 && st.st_gid == 0) ||
          ((st.st_mode & 07777) == 0440 && m_credentialGroup >= 0 &&
           st.st_gid == static_cast<gid_t>(m_credentialGroup))) ||
        st.st_size <= 0 ||
        st.st_size > static_cast<off_t>(MAX_CREDENTIAL_LENGTH))
    {
        throw RedisAuthError("Redis credential file failed validation");
    }

    try
    {
        readExactFile(credentialFd.get(), static_cast<size_t>(st.st_size),
                      "Redis credential file could not be read", credential);
        if (!isCanonicalBase64Url(credential))
        {
            throw RedisAuthError("Redis credential file failed validation");
        }
    }
    catch (...)
    {
        clearCredential(credential);
        throw;
    }
}

void RedisAuthConfig::clearCredential(string& credential) noexcept
{
    volatile char *data = credential.empty() ? nullptr : &credential[0];
    for (size_t i = 0; i < credential.size(); ++i)
    {
        data[i] = 0;
    }
    credential.clear();
}

}
