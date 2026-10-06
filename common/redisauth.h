#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace swss
{

class RedisContext;

class RedisAuthError : public std::runtime_error
{
public:
    explicit RedisAuthError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

/*
 * Non-secret metadata needed to authenticate a Redis connection.
 *
 * The credential itself is deliberately not retained in this object.  It is
 * read from the protected file for every new connection and cleared after the
 * AUTH exchange.
 */
class RedisAuthConfig
{
public:
    static constexpr const char *DEFAULT_CLIENT_PROFILES_FILE = "/run/redis-auth/client-profiles.json";

    RedisAuthConfig();

    /*
     * Construct an authentication policy bound to one exact target.  These
     * factories are intended for native services whose root-owned endpoint
     * configuration already supplies the credential metadata.  Command-line
     * tools should use fromProfile() instead.
     *
     * A negative credentialGroup requires root:root 0400.  A non-negative
     * value additionally permits root:<credentialGroup> 0440.
     */
    static RedisAuthConfig forTcp(
        const std::string& username,
        const std::string& credentialFile,
        const std::string& hostname,
        int port,
        int64_t credentialGroup = -1);
    static RedisAuthConfig forUnixSocket(
        const std::string& username,
        const std::string& credentialFile,
        const std::string& unixPath,
        int64_t credentialGroup = -1);

    /*
     * Resolve a non-secret, root-owned profile map with this schema:
     * {"schema_version":1,"profiles":{"name":{"username":"...",
     *  "domain":"...","credential_file":"/absolute/path",
     *  "endpoints":[{"transport":"tcp","hostname":"...","port":6379},
     *               {"transport":"unix","path":"/absolute/path"}]}}}
     */
    static RedisAuthConfig fromProfile(
        const std::string& profile,
        const std::string& profilesFile = DEFAULT_CLIENT_PROFILES_FILE);

    bool isConfigured() const;
    const std::string& getUsername() const;
    const std::string& getCredentialFile() const;
    const std::string& getDomain() const;
    bool operator==(const RedisAuthConfig& other) const;
    bool operator!=(const RedisAuthConfig& other) const
    {
        return !(*this == other);
    }

    /* Validate the selected target before a socket is opened or a secret read. */
    void validateTcpEndpoint(const std::string& hostname, int port) const;
    void validateUnixEndpoint(const std::string& unixPath) const;

private:
    friend class RedisContext;

    enum class EndpointType
    {
        TCP,
        UNIX_SOCKET
    };

    struct Endpoint
    {
        EndpointType type;
        std::string address;
        int port;
    };

    RedisAuthConfig(
        const std::string& username,
        const std::string& credentialFile,
        const std::string& domain,
        int64_t credentialGroup,
        std::vector<Endpoint> endpoints);

    void readCredential(std::string& credential) const;
    static void clearCredential(std::string& credential) noexcept;

    std::string m_username;
    std::string m_credentialFile;
    std::string m_domain;
    int64_t m_credentialGroup = -1;
    std::vector<Endpoint> m_endpoints;
};

}
