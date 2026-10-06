#include <arpa/inet.h>
#include <dirent.h>
#include <endian.h>
#include <fcntl.h>
#include <linux/posix_acl.h>
#include <linux/posix_acl_xattr.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "common/c-api/configdbconnector.h"
#include "common/c-api/dbconnector.h"
#include "common/c-api/sonicv2connector.h"
#include "common/dbconnector.h"
#include "common/dbinterface.h"
#include "common/notificationconsumer.h"
#include "common/redisauth.h"
#include "common/redisreply.h"

using namespace swss;

namespace
{

constexpr char WRITER_USER[] = "sonic-trusted-writer";
constexpr char ALTERNATE_USER[] = "sonic-database-client";
constexpr char WRITER_CREDENTIAL[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
const std::string WRONG_CREDENTIAL = std::string(42, 'B') + "A";

static_assert(sizeof(WRITER_CREDENTIAL) - 1 == 43,
              "The test credential must be canonical unpadded base64url");
static_assert(sizeof(RedisContext) == sizeof(void *),
              "RedisContext must retain its historical ABI size");

/*
 * These compile-time checks cover the named-database overloads without changing
 * the process-wide SonicDBConfig used by the rest of the test binary.
 */
static_assert(std::is_constructible<DBConnector, const std::string&, unsigned int,
                                    bool, const RedisAuthConfig&>::value,
              "Named DB connector must accept an authentication profile");
static_assert(std::is_constructible<DBConnector, const std::string&, unsigned int,
                                    bool, const std::string&,
                                    const RedisAuthConfig&>::value,
              "Namespaced DB connector must accept an authentication profile");
static_assert(std::is_constructible<DBConnector, const std::string&, unsigned int,
                                    bool, const SonicDBKey&,
                                    const RedisAuthConfig&>::value,
              "Smart-switch DB connector must accept an authentication profile");

void writeFile(const std::string& path, const std::string& contents, mode_t mode)
{
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    if (fd < 0)
    {
        throw std::runtime_error("Unable to create test file " + path + ": " +
                                 std::strerror(errno));
    }

    size_t offset = 0;
    while (offset < contents.size())
    {
        ssize_t rc = write(fd, contents.data() + offset, contents.size() - offset);
        if (rc < 0 && errno == EINTR)
        {
            continue;
        }
        if (rc <= 0)
        {
            int savedErrno = errno;
            close(fd);
            throw std::runtime_error("Unable to write test file " + path + ": " +
                                     std::strerror(savedErrno));
        }
        offset += static_cast<size_t>(rc);
    }

    if (fchmod(fd, mode) != 0)
    {
        int savedErrno = errno;
        close(fd);
        throw std::runtime_error("Unable to set permissions on test file " + path +
                                 ": " + std::strerror(savedErrno));
    }

    if (close(fd) != 0)
    {
        throw std::runtime_error("Unable to close test file " + path + ": " +
                                 std::strerror(errno));
    }
}

std::string readFile(const std::string& path)
{
    std::ifstream input(path);
    if (!input)
    {
        return std::string();
    }

    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

int reserveLoopbackPort()
{
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        throw std::runtime_error("Unable to create a socket for Redis test port selection");
    }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);

    if (bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
    {
        int savedErrno = errno;
        close(fd);
        throw std::runtime_error("Unable to reserve a Redis test port: " +
                                 std::string(std::strerror(savedErrno)));
    }

    socklen_t length = sizeof(address);
    if (getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length) != 0)
    {
        int savedErrno = errno;
        close(fd);
        throw std::runtime_error("Unable to read the Redis test port: " +
                                 std::string(std::strerror(savedErrno)));
    }

    int port = ntohs(address.sin_port);
    close(fd);
    return port;
}

class CaptureListener
{
public:
    CaptureListener()
        : m_fd(socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0))
        , m_port(0)
    {
        if (m_fd < 0)
        {
            throw std::runtime_error("Unable to create capture listener");
        }

        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(0);
        if (bind(m_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
            listen(m_fd, 1) != 0)
        {
            int savedErrno = errno;
            close(m_fd);
            throw std::runtime_error("Unable to start capture listener: " +
                                     std::string(std::strerror(savedErrno)));
        }

        socklen_t length = sizeof(address);
        if (getsockname(m_fd, reinterpret_cast<sockaddr *>(&address), &length) != 0)
        {
            int savedErrno = errno;
            close(m_fd);
            throw std::runtime_error("Unable to read capture listener port: " +
                                     std::string(std::strerror(savedErrno)));
        }
        m_port = ntohs(address.sin_port);
    }

    ~CaptureListener()
    {
        close(m_fd);
    }

    int port() const
    {
        return m_port;
    }

    bool acceptedConnection() const
    {
        int client = accept(m_fd, nullptr, nullptr);
        if (client >= 0)
        {
            close(client);
            return true;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            throw std::runtime_error("Capture listener accept failed");
        }
        return false;
    }

private:
    int m_fd;
    int m_port;
};

class DisposableRedisServer
{
public:
    explicit DisposableRedisServer(bool permissiveDefault,
                                   bool readOnlyDefault = false)
        : m_pid(-1)
        , m_port(reserveLoopbackPort())
    {
        char directoryTemplate[] = "/tmp/swss-redis-auth-XXXXXX";
        char *directory = mkdtemp(directoryTemplate);
        if (directory == nullptr)
        {
            throw std::runtime_error("Unable to create Redis test directory");
        }

        m_directory = directory;
        m_socketPath = path("redis.sock");
        m_logPath = path("redis.log");

        std::ostringstream acl;
        if (permissiveDefault)
        {
            acl << "user default on nopass ~* &* +@all\n";
        }
        else if (readOnlyDefault)
        {
            acl << "user default on nopass ~* &* +@read +@connection "
                   "+subscribe +psubscribe +unsubscribe +punsubscribe\n";
        }
        else
        {
            acl << "user default off\n";
        }
        acl << "user " << WRITER_USER << " on >" << WRITER_CREDENTIAL
            << " ~* &* +@all\n"
            << "user " << ALTERNATE_USER << " on >" << WRITER_CREDENTIAL
            << " ~* &* +@all\n";
        writeFile(path("users.acl"), acl.str(), 0600);

        std::ostringstream config;
        config << "bind 127.0.0.1\n"
               << "protected-mode no\n"
               << "port " << m_port << "\n"
               << "unixsocket " << m_socketPath << "\n"
               << "unixsocketperm 700\n"
               << "daemonize no\n"
               << "supervised no\n"
               << "save \"\"\n"
               << "appendonly no\n"
               << "notify-keyspace-events AKE\n"
               << "databases 16\n"
               << "dir " << m_directory << "\n"
               << "dbfilename dump.rdb\n"
               << "aclfile " << path("users.acl") << "\n"
               << "logfile \"\"\n";
        std::string configPath = path("redis.conf");
        writeFile(configPath, config.str(), 0600);

        m_pid = fork();
        if (m_pid < 0)
        {
            cleanupDirectory();
            throw std::runtime_error("Unable to fork redis-server");
        }

        if (m_pid == 0)
        {
            int logFd = open(m_logPath.c_str(),
                             O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
            if (logFd >= 0)
            {
                dup2(logFd, STDOUT_FILENO);
                dup2(logFd, STDERR_FILENO);
                close(logFd);
            }

            execlp("redis-server", "redis-server", configPath.c_str(),
                   static_cast<char *>(nullptr));
            _exit(127);
        }

        try
        {
            waitUntilReady();
        }
        catch (...)
        {
            stop();
            std::string log = readFile(m_logPath);
            cleanupDirectory();
            throw std::runtime_error("Disposable redis-server did not start: " + log);
        }
    }

    ~DisposableRedisServer()
    {
        stop();
        cleanupDirectory();
    }

    DisposableRedisServer(const DisposableRedisServer&) = delete;
    DisposableRedisServer& operator=(const DisposableRedisServer&) = delete;

    int port() const
    {
        return m_port;
    }

    const std::string& socketPath() const
    {
        return m_socketPath;
    }

    std::string path(const std::string& filename) const
    {
        return m_directory + "/" + filename;
    }

    std::string credentialFile(const std::string& filename,
                               const std::string& credential,
                               mode_t mode = 0400) const
    {
        std::string filenamePath = path(filename);
        writeFile(filenamePath, credential, mode);
        return filenamePath;
    }

private:
    bool endpointReady(bool unixSocket) const
    {
        timeval timeout = {0, 50000};
        redisContext *context = unixSocket
            ? redisConnectUnixWithTimeout(m_socketPath.c_str(), timeout)
            : redisConnectWithTimeout("127.0.0.1", m_port, timeout);

        if (context == nullptr)
        {
            return false;
        }

        bool ready = context->err == 0;
        if (ready)
        {
            redisReply *reply = static_cast<redisReply *>(redisCommand(context, "PING"));
            ready = reply != nullptr;
            if (reply != nullptr)
            {
                freeReplyObject(reply);
            }
        }

        redisFree(context);
        return ready;
    }

    void waitUntilReady()
    {
        for (int attempt = 0; attempt < 250; ++attempt)
        {
            int status = 0;
            pid_t result = waitpid(m_pid, &status, WNOHANG);
            if (result == m_pid)
            {
                m_pid = -1;
                throw std::runtime_error("redis-server exited during startup");
            }

            if (endpointReady(false) && endpointReady(true))
            {
                return;
            }

            usleep(20000);
        }

        throw std::runtime_error("redis-server readiness timed out");
    }

    void stop() noexcept
    {
        if (m_pid <= 0)
        {
            return;
        }

        kill(m_pid, SIGTERM);
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            int status = 0;
            pid_t result = waitpid(m_pid, &status, WNOHANG);
            if (result == m_pid || (result < 0 && errno == ECHILD))
            {
                m_pid = -1;
                return;
            }
            usleep(10000);
        }

        kill(m_pid, SIGKILL);
        int status = 0;
        while (waitpid(m_pid, &status, 0) < 0 && errno == EINTR)
        {
        }
        m_pid = -1;
    }

    void cleanupDirectory() noexcept
    {
        if (m_directory.empty())
        {
            return;
        }

        DIR *directory = opendir(m_directory.c_str());
        if (directory != nullptr)
        {
            dirent *entry;
            while ((entry = readdir(directory)) != nullptr)
            {
                if (std::strcmp(entry->d_name, ".") == 0 ||
                    std::strcmp(entry->d_name, "..") == 0)
                {
                    continue;
                }

                std::string child = path(entry->d_name);
                if (unlink(child.c_str()) != 0 && errno == EISDIR)
                {
                    rmdir(child.c_str());
                }
            }
            closedir(directory);
        }

        rmdir(m_directory.c_str());
        m_directory.clear();
    }

    pid_t m_pid;
    int m_port;
    std::string m_directory;
    std::string m_socketPath;
    std::string m_logPath;
};

std::string authenticatedUser(DBConnector& connector)
{
    RedisReply reply(&connector, "ACL WHOAMI", REDIS_REPLY_STRING);
    return reply.getReply<std::string>();
}

long long commandCallCount(DBConnector& connector, const std::string& command)
{
    RedisReply reply(&connector, "INFO commandstats", REDIS_REPLY_STRING);
    const std::string info = reply.getReply<std::string>();
    const std::string prefix = "cmdstat_" + command + ":calls=";
    const std::string::size_type start = info.find(prefix);
    if (start == std::string::npos)
    {
        return 0;
    }

    const std::string::size_type valueStart = start + prefix.size();
    const std::string::size_type valueEnd = info.find(',', valueStart);
    return std::stoll(info.substr(valueStart, valueEnd - valueStart));
}

bool waitForCommandCalls(DBConnector& connector, const std::string& command,
                         long long minimumCalls)
{
    for (int attempt = 0; attempt < 5000; ++attempt)
    {
        if (commandCallCount(connector, command) >= minimumCalls)
        {
            return true;
        }
        usleep(1000);
    }
    return false;
}

template <typename Action>
void expectRedisAuthError(Action action,
                          const std::string& expectedMessage,
                          const std::vector<std::string>& forbiddenText)
{
    bool caught = false;
    try
    {
        action();
    }
    catch (const RedisAuthError& error)
    {
        caught = true;
        EXPECT_EQ(expectedMessage, error.what());
        for (const auto& text : forbiddenText)
        {
            EXPECT_EQ(std::string::npos, std::string(error.what()).find(text));
        }
    }
    catch (const std::exception& error)
    {
        caught = true;
        ADD_FAILURE() << "Expected RedisAuthError, got: " << error.what();
    }

    EXPECT_TRUE(caught) << "Expected RedisAuthError";
}

void freeResultStrings(SWSSResult& result)
{
    if (result.message != nullptr)
    {
        SWSSString_free(result.message);
        result.message = nullptr;
    }
    if (result.location != nullptr)
    {
        SWSSString_free(result.location);
        result.location = nullptr;
    }
}

class RedisAuthTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_EQ(static_cast<uid_t>(0), geteuid())
            << "Redis credential validation tests must run as root";
    }

    void startServer(bool permissiveDefault)
    {
        m_server.reset(new DisposableRedisServer(permissiveDefault));
        m_validCredentialFile = m_server->credentialFile(
            "writer.secret", WRITER_CREDENTIAL, 0400);

        struct stat st = {};
        ASSERT_EQ(0, stat(m_validCredentialFile.c_str(), &st));
        EXPECT_EQ(static_cast<uid_t>(0), st.st_uid);
        EXPECT_EQ(static_cast<mode_t>(0400), st.st_mode & 0777);
    }

    void startReadOnlyServer()
    {
        m_server.reset(new DisposableRedisServer(false, true));
        m_validCredentialFile = m_server->credentialFile(
            "writer.secret", WRITER_CREDENTIAL, 0400);
    }

    RedisAuthConfig authFor(bool tcp,
                            const std::string& credentialFile,
                            const std::string& username = WRITER_USER,
                            int64_t credentialGroup = -1) const
    {
        if (tcp)
        {
            return RedisAuthConfig::forTcp(
                username, credentialFile, "127.0.0.1", m_server->port(),
                credentialGroup);
        }

        return RedisAuthConfig::forUnixSocket(
            username, credentialFile, m_server->socketPath(), credentialGroup);
    }

    RedisAuthConfig validAuth(bool tcp) const
    {
        return authFor(tcp, m_validCredentialFile);
    }

    std::unique_ptr<DBConnector> connect(int dbId, bool tcp,
                                         const RedisAuthConfig& auth) const
    {
        if (tcp)
        {
            return std::unique_ptr<DBConnector>(
                new DBConnector(dbId, "127.0.0.1", m_server->port(), 1000, auth));
        }

        return std::unique_ptr<DBConnector>(
            new DBConnector(dbId, m_server->socketPath(), 1000, auth));
    }

    std::unique_ptr<DBConnector> connectWithoutAuth(int dbId, bool tcp) const
    {
        if (tcp)
        {
            return std::unique_ptr<DBConnector>(
                new DBConnector(dbId, "127.0.0.1", m_server->port(), 1000));
        }

        return std::unique_ptr<DBConnector>(
            new DBConnector(dbId, m_server->socketPath(), 1000));
    }

    void verifyAuthenticatedSelection(DBConnector& connector,
                                      const std::string& key,
                                      const std::string& value,
                                      const std::string& expectedUser = WRITER_USER)
    {
        EXPECT_EQ(expectedUser, authenticatedUser(connector));
        ASSERT_TRUE(connector.set(key, value));
        std::shared_ptr<std::string> actual = connector.get(key);
        ASSERT_TRUE(actual);
        EXPECT_EQ(value, *actual);
    }

    void exercisePreservedAuthentication(bool tcp)
    {
        startServer(false);
        RedisAuthConfig auth = validAuth(tcp);

        std::unique_ptr<DBConnector> original = connect(6, tcp, auth);
        verifyAuthenticatedSelection(*original, "db-six-anchor", "present");

        DBConnector copied(*original);
        EXPECT_EQ(WRITER_USER, authenticatedUser(copied));
        ASSERT_TRUE(copied.get("db-six-anchor"));

        std::unique_ptr<DBConnector> recreated(original->newConnector(1000));
        EXPECT_EQ(WRITER_USER, authenticatedUser(*recreated));
        ASSERT_TRUE(recreated->get("db-six-anchor"));

        original->reconnect();
        EXPECT_EQ(WRITER_USER, authenticatedUser(*original));
        ASSERT_TRUE(original->get("db-six-anchor"));

        DBConnector otherDatabase(7, static_cast<const RedisContext&>(*original));
        verifyAuthenticatedSelection(otherDatabase, "db-seven-anchor", "present");
        EXPECT_FALSE(otherDatabase.get("db-six-anchor"));

        std::unique_ptr<DBConnector> verifyDbSeven = connect(7, tcp, auth);
        ASSERT_TRUE(verifyDbSeven->get("db-seven-anchor"));
    }

    std::unique_ptr<DisposableRedisServer> m_server;
    std::string m_validCredentialFile;
};

TEST_F(RedisAuthTest, LegacyConnectionsWithoutProfileUseTcpAndUnixSocket)
{
    startServer(true);

    std::unique_ptr<DBConnector> tcp = connectWithoutAuth(3, true);
    EXPECT_EQ("default", authenticatedUser(*tcp));
    ASSERT_TRUE(tcp->set("legacy-key", "legacy-value"));

    std::unique_ptr<DBConnector> unixSocket = connectWithoutAuth(3, false);
    EXPECT_EQ("default", authenticatedUser(*unixSocket));
    std::shared_ptr<std::string> value = unixSocket->get("legacy-key");
    ASSERT_TRUE(value);
    EXPECT_EQ("legacy-value", *value);
}

TEST_F(RedisAuthTest, AuthenticatedTcpConnectionAuthenticatesBeforeSelect)
{
    startServer(false);

    std::unique_ptr<DBConnector> connector = connect(5, true, validAuth(true));
    verifyAuthenticatedSelection(*connector, "tcp-auth-key", "tcp-auth-value");

    std::unique_ptr<DBConnector> dbZero = connect(0, true, validAuth(true));
    EXPECT_FALSE(dbZero->get("tcp-auth-key"));
}

TEST_F(RedisAuthTest, AuthenticatedUnixConnectionAuthenticatesBeforeSelect)
{
    startServer(false);

    std::unique_ptr<DBConnector> connector = connect(5, false, validAuth(false));
    verifyAuthenticatedSelection(*connector, "unix-auth-key", "unix-auth-value");

    std::unique_ptr<DBConnector> dbZero = connect(0, false, validAuth(false));
    EXPECT_FALSE(dbZero->get("unix-auth-key"));
}

TEST_F(RedisAuthTest, EnforcedServerRejectsUnconfiguredConnections)
{
    startServer(false);

    EXPECT_THROW(connectWithoutAuth(5, true), std::system_error);
    EXPECT_THROW(connectWithoutAuth(5, false), std::system_error);
}

TEST_F(RedisAuthTest, WrongCredentialFailsClosedAndRedactsBothTransports)
{
    startServer(true);
    std::string wrongFile = m_server->credentialFile(
        "wrong.secret", WRONG_CREDENTIAL, 0400);
    RedisAuthConfig wrongTcpAuth = authFor(true, wrongFile);
    RedisAuthConfig wrongUnixAuth = authFor(false, wrongFile);

    expectRedisAuthError(
        [&] { connect(5, true, wrongTcpAuth); },
        "Redis authentication failed",
        {WRITER_CREDENTIAL, WRONG_CREDENTIAL});
    expectRedisAuthError(
        [&] { connect(5, false, wrongUnixAuth); },
        "Redis authentication failed",
        {WRITER_CREDENTIAL, WRONG_CREDENTIAL});

    /* A failed named AUTH must not silently continue as permissive default. */
    std::unique_ptr<DBConnector> legacy = connectWithoutAuth(5, true);
    EXPECT_EQ("default", authenticatedUser(*legacy));
    EXPECT_TRUE(legacy->set("default-still-permissive", "yes"));
}

TEST_F(RedisAuthTest, UnknownUserFailsClosedAndRedactsCredential)
{
    startServer(true);
    RedisAuthConfig unknownUser = authFor(true, m_validCredentialFile, "unknown-writer");

    expectRedisAuthError(
        [&] { connect(5, true, unknownUser); },
        "Redis authentication failed",
        {WRITER_CREDENTIAL});
}

TEST_F(RedisAuthTest, AuthenticationPolicyRejectsWrongEndpointBeforeConnect)
{
    startServer(true);

    CaptureListener listener;
    RedisAuthConfig serverOnlyAuth = validAuth(true);
    expectRedisAuthError(
        [&]
        {
            DBConnector connector(
                5, "127.0.0.1", listener.port(), 100, serverOnlyAuth);
        },
        "Redis authentication profile does not allow target endpoint",
        {WRITER_CREDENTIAL, m_validCredentialFile});
    EXPECT_FALSE(listener.acceptedConnection());

    RedisAuthConfig wrongTcpTarget = RedisAuthConfig::forTcp(
        WRITER_USER, m_validCredentialFile, "127.0.0.1", m_server->port() + 1);
    expectRedisAuthError(
        [&] { connect(5, true, wrongTcpTarget); },
        "Redis authentication profile does not allow target endpoint",
        {WRITER_CREDENTIAL, m_validCredentialFile});

    RedisAuthConfig wrongUnixTarget = RedisAuthConfig::forUnixSocket(
        WRITER_USER, m_validCredentialFile, m_server->path("other.sock"));
    expectRedisAuthError(
        [&] { connect(5, false, wrongUnixTarget); },
        "Redis authentication profile does not allow target endpoint",
        {WRITER_CREDENTIAL, m_validCredentialFile});
}

TEST_F(RedisAuthTest, CopyNewConnectorAndReconnectPreserveTcpAuthentication)
{
    exercisePreservedAuthentication(true);
}

TEST_F(RedisAuthTest, CopyNewConnectorAndReconnectPreserveUnixAuthentication)
{
    exercisePreservedAuthentication(false);
}

TEST_F(RedisAuthTest, ReconnectAuthenticationFailureClosesConnection)
{
    startServer(true);
    std::unique_ptr<DBConnector> connector = connect(5, true, validAuth(true));
    EXPECT_EQ(WRITER_USER, authenticatedUser(*connector));

    writeFile(m_validCredentialFile, WRONG_CREDENTIAL, 0400);
    expectRedisAuthError(
        [&] { connector->reconnect(); },
        "Redis authentication failed",
        {WRITER_CREDENTIAL, WRONG_CREDENTIAL, m_validCredentialFile});
    EXPECT_EQ(nullptr, connector->getContext());
    expectRedisAuthError(
        [&] { std::unique_ptr<DBConnector> unused(connector->newConnector(1000)); },
        "Cannot recreate a closed Redis connection",
        {WRITER_CREDENTIAL, WRONG_CREDENTIAL, m_validCredentialFile});
}

TEST_F(RedisAuthTest, DBInterfaceConnectWithAuthPreservesAuthentication)
{
    startServer(false);
    DBInterface interface;
    interface.set_redis_kwargs("", "127.0.0.1", m_server->port());
    interface.connect_with_auth(5, "AUTH_DB", false, validAuth(true));

    DBConnector& connector = interface.get_redis_client("AUTH_DB");
    EXPECT_EQ(WRITER_USER, authenticatedUser(connector));
    EXPECT_TRUE(connector.set("db-interface-auth", "present"));
}

TEST_F(RedisAuthTest, DBInterfaceLegacyConnectDoesNotRequireServerWritePermission)
{
    startReadOnlyServer();

    std::unique_ptr<DBConnector> writer = connect(5, true, validAuth(true));
    ASSERT_TRUE(writer->set("read-only-default-key", "present"));

    DBInterface tcpInterface;
    tcpInterface.set_redis_kwargs("", "127.0.0.1", m_server->port());
    ASSERT_NO_THROW(tcpInterface.connect(5, "READ_ONLY_TCP", false));
    std::shared_ptr<std::string> tcpValue =
        tcpInterface.get_redis_client("READ_ONLY_TCP").get("read-only-default-key");
    ASSERT_TRUE(tcpValue);
    EXPECT_EQ("present", *tcpValue);
    EXPECT_THROW(
        tcpInterface.get_redis_client("READ_ONLY_TCP").set(
            "unauthenticated-write", "denied"),
        std::system_error);

    const long long tcpHgetCalls = commandCallCount(*writer, "hget");
    bool tcpReachedNotificationWait = false;
    std::thread tcpWriter(
        [&writer, tcpHgetCalls, &tcpReachedNotificationWait]
        {
            tcpReachedNotificationWait =
                waitForCommandCalls(*writer, "hget", tcpHgetCalls + 2);
            writer->hset("blocking-tcp", "field", "arrived");
        });
    std::shared_ptr<std::string> blockingTcpValue;
    try
    {
        blockingTcpValue = tcpInterface.get(
            "READ_ONLY_TCP", "blocking-tcp", "field", true);
    }
    catch (...)
    {
        tcpWriter.join();
        throw;
    }
    tcpWriter.join();
    EXPECT_TRUE(tcpReachedNotificationWait);
    ASSERT_TRUE(blockingTcpValue);
    EXPECT_EQ("arrived", *blockingTcpValue);

    DBInterface unixInterface;
    unixInterface.set_redis_kwargs(m_server->socketPath(), "", 0);
    ASSERT_NO_THROW(unixInterface.connect(5, "READ_ONLY_UNIX", false));
    std::shared_ptr<std::string> unixValue =
        unixInterface.get_redis_client("READ_ONLY_UNIX").get("read-only-default-key");
    ASSERT_TRUE(unixValue);
    EXPECT_EQ("present", *unixValue);
    EXPECT_THROW(
        unixInterface.get_redis_client("READ_ONLY_UNIX").set(
            "unauthenticated-write", "denied"),
        std::system_error);

    const long long unixHgetCalls = commandCallCount(*writer, "hget");
    bool unixReachedNotificationWait = false;
    std::thread unixWriter(
        [&writer, unixHgetCalls, &unixReachedNotificationWait]
        {
            unixReachedNotificationWait =
                waitForCommandCalls(*writer, "hget", unixHgetCalls + 2);
            writer->hset("blocking-unix", "field", "arrived");
        });
    std::shared_ptr<std::string> blockingUnixValue;
    try
    {
        blockingUnixValue = unixInterface.get(
            "READ_ONLY_UNIX", "blocking-unix", "field", true);
    }
    catch (...)
    {
        unixWriter.join();
        throw;
    }
    unixWriter.join();
    EXPECT_TRUE(unixReachedNotificationWait);
    ASSERT_TRUE(blockingUnixValue);
    EXPECT_EQ("arrived", *blockingUnixValue);
}

TEST_F(RedisAuthTest, DBInterfaceRejectsAuthenticationPolicyChangeOnOpenConnection)
{
    startServer(true);
    DBInterface interface;
    interface.set_redis_kwargs("", "127.0.0.1", m_server->port());
    interface.connect(5, "AUTH_DB", false);

    EXPECT_THROW(
        interface.connect_with_auth(5, "AUTH_DB", false, validAuth(true)),
        std::logic_error);
    EXPECT_EQ("default", authenticatedUser(interface.get_redis_client("AUTH_DB")));
}

TEST_F(RedisAuthTest, NotificationConsumerTreatsAuthenticationFailureAsTerminal)
{
    startServer(true);
    std::unique_ptr<DBConnector> connector = connect(5, true, validAuth(true));
    writeFile(m_validCredentialFile, WRONG_CREDENTIAL, 0400);

    expectRedisAuthError(
        [&] { NotificationConsumer consumer(connector.get(), "AUTH_CHANNEL"); },
        "Redis authentication failed",
        {WRITER_CREDENTIAL, WRONG_CREDENTIAL, m_validCredentialFile});
}

TEST_F(RedisAuthTest, CredentialFileValidationRejectsUnsafeInputs)
{
    startServer(true);

    std::string missing = m_server->path("missing.secret");
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, missing)); },
        "Redis credential file could not be opened",
        {WRITER_CREDENTIAL});

    std::string shortFile = m_server->credentialFile(
        "short.secret", std::string(42, 'A'), 0400);
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, shortFile)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL});

    std::string longFile = m_server->credentialFile(
        "long.secret", std::string(129, 'A'), 0400);
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, longFile)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL});

    std::string newlineFile = m_server->credentialFile(
        "newline.secret", std::string(WRITER_CREDENTIAL) + "\n", 0400);
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, newlineFile)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL});

    std::string invalidAlphabet(WRITER_CREDENTIAL);
    invalidAlphabet[20] = '+';
    std::string alphabetFile = m_server->credentialFile(
        "alphabet.secret", invalidAlphabet, 0400);
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, alphabetFile)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL, invalidAlphabet});

    std::string impossibleLengthFile = m_server->credentialFile(
        "impossible-length.secret", std::string(45, 'A'), 0400);
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, impossibleLengthFile)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL});

    std::string noncanonicalTail = std::string(42, 'A') + "B";
    std::string noncanonicalFile = m_server->credentialFile(
        "noncanonical.secret", noncanonicalTail, 0400);
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, noncanonicalFile)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL, noncanonicalTail});

    std::string badModeFile = m_server->credentialFile(
        "mode.secret", WRITER_CREDENTIAL, 0644);
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, badModeFile)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL});

    std::string symlinkPath = m_server->path("linked.secret");
    ASSERT_EQ(0, symlink(m_validCredentialFile.c_str(), symlinkPath.c_str()));
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, symlinkPath)); },
        "Redis credential file could not be opened",
        {WRITER_CREDENTIAL});

    std::string symlinkParent = m_server->path("linked-parent");
    ASSERT_EQ(0, symlink(m_server->path(".").c_str(), symlinkParent.c_str()));
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, symlinkParent + "/writer.secret")); },
        "Redis credential file could not be opened",
        {WRITER_CREDENTIAL});

    std::string hardlinkPath = m_server->path("hardlinked.secret");
    ASSERT_EQ(0, link(m_validCredentialFile.c_str(), hardlinkPath.c_str()));
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, hardlinkPath)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL});
}

TEST_F(RedisAuthTest, CredentialFileMode0440IsAccepted)
{
    startServer(false);
    std::string groupReadableFile = m_server->credentialFile(
        "writer-group.secret", WRITER_CREDENTIAL, 0440);
    RedisAuthConfig auth = authFor(true, groupReadableFile, WRITER_USER, 0);

    std::unique_ptr<DBConnector> connector = connect(5, true, auth);
    EXPECT_EQ(WRITER_USER, authenticatedUser(*connector));
}

TEST_F(RedisAuthTest, CredentialFileMode0440RequiresExpectedGroup)
{
    startServer(true);
    std::string groupReadableFile = m_server->credentialFile(
        "writer-group.secret", WRITER_CREDENTIAL, 0440);

    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, groupReadableFile)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL});
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, groupReadableFile, WRITER_USER, 1)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL});
}

TEST_F(RedisAuthTest, CredentialFileRejectsExtendedAccessAcl)
{
    startServer(true);
    std::string aclFile = m_server->credentialFile(
        "writer-acl.secret", WRITER_CREDENTIAL, 0440);

    struct AccessAcl
    {
        posix_acl_xattr_header header;
        posix_acl_xattr_entry entries[5];
    } acl = {};

    acl.header.a_version = htole32(POSIX_ACL_XATTR_VERSION);
    auto setEntry = [&](size_t index, uint16_t tag, uint16_t permissions,
                        uint32_t id)
    {
        acl.entries[index].e_tag = htole16(tag);
        acl.entries[index].e_perm = htole16(permissions);
        acl.entries[index].e_id = htole32(id);
    };
    const uint32_t undefinedId = static_cast<uint32_t>(ACL_UNDEFINED_ID);
    setEntry(0, ACL_USER_OBJ, ACL_READ, undefinedId);
    setEntry(1, ACL_USER, ACL_READ, 65534);
    setEntry(2, ACL_GROUP_OBJ, ACL_READ, undefinedId);
    setEntry(3, ACL_MASK, ACL_READ, undefinedId);
    setEntry(4, ACL_OTHER, 0, undefinedId);

    if (setxattr(aclFile.c_str(), "system.posix_acl_access", &acl, sizeof(acl), 0) != 0)
    {
        if (errno == ENOTSUP
#if EOPNOTSUPP != ENOTSUP
            || errno == EOPNOTSUPP
#endif
        )
        {
            GTEST_SKIP() << "Test filesystem does not support POSIX access ACLs";
        }
        FAIL() << "Unable to install test POSIX ACL: " << std::strerror(errno);
    }

    struct stat st = {};
    ASSERT_EQ(0, stat(aclFile.c_str(), &st));
    ASSERT_EQ(static_cast<mode_t>(0440), st.st_mode & 0777);
    expectRedisAuthError(
        [&] { connect(5, true, authFor(true, aclFile, WRITER_USER, 0)); },
        "Redis credential file failed validation",
        {WRITER_CREDENTIAL, aclFile});
}

TEST_F(RedisAuthTest, AuthenticationConfigRejectsInvalidMetadata)
{
    EXPECT_THROW(RedisAuthConfig::forTcp(
                     WRITER_USER, "relative.secret", "127.0.0.1", 6379),
                 std::invalid_argument);
    EXPECT_THROW(RedisAuthConfig::forTcp(
                     "invalid user", "/tmp/unused.secret", "127.0.0.1", 6379),
                 std::invalid_argument);
    EXPECT_THROW(RedisAuthConfig::forTcp(
                     "", "/tmp/unused.secret", "127.0.0.1", 6379),
                 std::invalid_argument);
    EXPECT_THROW(RedisAuthConfig::forTcp(
                     WRITER_USER, "/tmp/unused.secret", "", 6379),
                 std::invalid_argument);
    EXPECT_THROW(RedisAuthConfig::forTcp(
                     WRITER_USER, "/tmp/unused.secret", "127.0.0.1", 0),
                 std::invalid_argument);

    RedisAuthConfig noAuth;
    EXPECT_FALSE(noAuth.isConfigured());
}

TEST_F(RedisAuthTest, WriterProfileResolvesApprovedCredential)
{
    startServer(false);

    std::string profilesFile = m_server->path("client-profiles.json");
    std::ostringstream profiles;
    profiles << "{\n"
             << "  \"schema_version\": 1,\n"
             << "  \"profiles\": {\n"
             << "    \"local\": {\n"
             << "      \"username\": \"" << WRITER_USER << "\",\n"
             << "      \"domain\": \"test-system\",\n"
             << "      \"credential_file\": \"" << m_validCredentialFile << "\",\n"
             << "      \"endpoints\": [\n"
             << "        {\"transport\":\"tcp\",\"hostname\":\"127.0.0.1\",\"port\":"
             << m_server->port() << "},\n"
             << "        {\"transport\":\"unix\",\"path\":\""
             << m_server->socketPath() << "\"}\n"
             << "      ]\n"
             << "    }\n"
             << "  }\n"
             << "}\n";
    writeFile(profilesFile, profiles.str(), 0444);

    RedisAuthConfig auth = RedisAuthConfig::fromProfile("local", profilesFile);
    EXPECT_EQ(WRITER_USER, auth.getUsername());
    EXPECT_EQ(m_validCredentialFile, auth.getCredentialFile());
    EXPECT_EQ("test-system", auth.getDomain());

    std::unique_ptr<DBConnector> connector = connect(5, true, auth);
    verifyAuthenticatedSelection(*connector, "profile-auth-key", "profile-auth-value");
}

TEST_F(RedisAuthTest, ProfileAcceptsAnotherNamedAclIdentity)
{
    startServer(false);

    std::string profilesFile = m_server->path("named-client-profiles.json");
    std::ostringstream profiles;
    profiles << "{\"schema_version\":1,\"profiles\":{\"local\":{"
             << "\"username\":\"" << ALTERNATE_USER << "\","
             << "\"domain\":\"test-system\","
             << "\"credential_file\":\"" << m_validCredentialFile << "\","
             << "\"endpoints\":[{\"transport\":\"tcp\","
             << "\"hostname\":\"127.0.0.1\",\"port\":" << m_server->port()
             << "}]}}}";
    writeFile(profilesFile, profiles.str(), 0444);

    RedisAuthConfig auth = RedisAuthConfig::fromProfile("local", profilesFile);
    EXPECT_EQ(ALTERNATE_USER, auth.getUsername());

    std::unique_ptr<DBConnector> connector = connect(5, true, auth);
    verifyAuthenticatedSelection(
        *connector, "named-profile-key", "named-profile-value", ALTERNATE_USER);
}

TEST_F(RedisAuthTest, CApiProfileConnectorsAuthenticateOverTcpAndUnix)
{
    startServer(false);

    std::string profilesFile = m_server->path("c-api-client-profiles.json");
    std::ostringstream profiles;
    profiles << "{\"schema_version\":1,\"profiles\":{\"local\":{"
             << "\"username\":\"" << WRITER_USER << "\","
             << "\"domain\":\"c-api-test\","
             << "\"credential_file\":\"" << m_validCredentialFile << "\","
             << "\"endpoints\":["
             << "{\"transport\":\"tcp\",\"hostname\":\"127.0.0.1\",\"port\":"
             << m_server->port() << "},"
             << "{\"transport\":\"unix\",\"path\":\""
             << m_server->socketPath() << "\"}]}}}";
    writeFile(profilesFile, profiles.str(), 0444);

    SWSSDBConnector tcp = nullptr;
    SWSSResult result = SWSSDBConnector_new_tcp_with_profile(
        5, "127.0.0.1", static_cast<uint16_t>(m_server->port()), 1000,
        "local", profilesFile.c_str(), &tcp);
    ASSERT_EQ(SWSSException_None, result.exception);
    ASSERT_NE(nullptr, tcp);
    verifyAuthenticatedSelection(*reinterpret_cast<DBConnector *>(tcp),
                                 "c-api-tcp-key", "c-api-tcp-value");
    result = SWSSDBConnector_free(tcp);
    EXPECT_EQ(SWSSException_None, result.exception);

    SWSSDBConnector unixSocket = nullptr;
    result = SWSSDBConnector_new_unix_with_profile(
        5, m_server->socketPath().c_str(), 1000, "local",
        profilesFile.c_str(), &unixSocket);
    ASSERT_EQ(SWSSException_None, result.exception);
    ASSERT_NE(nullptr, unixSocket);
    verifyAuthenticatedSelection(*reinterpret_cast<DBConnector *>(unixSocket),
                                 "c-api-unix-key", "c-api-unix-value");
    result = SWSSDBConnector_free(unixSocket);
    EXPECT_EQ(SWSSException_None, result.exception);
}

TEST_F(RedisAuthTest, CApiProfileEntryPointsFailClosedBeforeConnecting)
{
    startServer(true);
    std::string missingProfiles = m_server->path("missing-profiles.json");

    SWSSDBConnector db = nullptr;
    SWSSResult result = SWSSDBConnector_new_named_with_profile(
        "TEST_DB", 1000, 1, "local", missingProfiles.c_str(), &db);
    EXPECT_EQ(SWSSException_Exception, result.exception);
    EXPECT_EQ(nullptr, db);
    freeResultStrings(result);

    result = SWSSDBConnector_new_keyed_with_profile(
        "TEST_DB", 1000, 1, "", "", "local", missingProfiles.c_str(), &db);
    EXPECT_EQ(SWSSException_Exception, result.exception);
    EXPECT_EQ(nullptr, db);
    freeResultStrings(result);

    SWSSSonicV2Connector sonic = nullptr;
    result = SWSSSonicV2Connector_new(1, "", &sonic);
    ASSERT_EQ(SWSSException_None, result.exception);
    ASSERT_NE(nullptr, sonic);
    result = SWSSSonicV2Connector_connect_with_profile(
        sonic, "TEST_DB", 0, "local", missingProfiles.c_str());
    EXPECT_EQ(SWSSException_Exception, result.exception);
    freeResultStrings(result);
    result = SWSSSonicV2Connector_free(sonic);
    EXPECT_EQ(SWSSException_None, result.exception);

    SWSSConfigDBConnector config = nullptr;
    result = SWSSConfigDBConnector_new(1, "", &config);
    ASSERT_EQ(SWSSException_None, result.exception);
    ASSERT_NE(nullptr, config);
    result = SWSSConfigDBConnector_connect_with_profile(
        config, 0, 0, "local", missingProfiles.c_str());
    EXPECT_EQ(SWSSException_Exception, result.exception);
    freeResultStrings(result);
    result = SWSSConfigDBConnector_free(config);
    EXPECT_EQ(SWSSException_None, result.exception);
}

TEST_F(RedisAuthTest, WriterProfileRejectsUnsafeOrUnknownMappings)
{
    startServer(true);

    std::string profilesFile = m_server->path("client-profiles.json");
    std::ostringstream profiles;
    profiles << "{\"schema_version\":1,\"profiles\":{\"local\":{"
             << "\"username\":\"" << WRITER_USER << "\","
             << "\"domain\":\"test-system\","
             << "\"credential_file\":\"" << m_validCredentialFile << "\","
             << "\"endpoints\":[{\"transport\":\"tcp\","
             << "\"hostname\":\"127.0.0.1\",\"port\":" << m_server->port()
             << "}]}}}";

    writeFile(profilesFile, profiles.str(), 0644);
    expectRedisAuthError(
        [&] { RedisAuthConfig::fromProfile("local", profilesFile); },
        "Redis authentication profile file failed validation",
        {WRITER_CREDENTIAL});

    writeFile(profilesFile, profiles.str(), 0444);
    expectRedisAuthError(
        [&] { RedisAuthConfig::fromProfile("unknown", profilesFile); },
        "Redis authentication profile was not found",
        {WRITER_CREDENTIAL});

    std::string symlinkPath = m_server->path("profiles-link.json");
    ASSERT_EQ(0, symlink(profilesFile.c_str(), symlinkPath.c_str()));
    expectRedisAuthError(
        [&] { RedisAuthConfig::fromProfile("local", symlinkPath); },
        "Redis authentication profile file could not be opened",
        {WRITER_CREDENTIAL});

    std::string oversizedVersion = profiles.str();
    size_t versionPosition = oversizedVersion.find("\"schema_version\":1");
    ASSERT_NE(std::string::npos, versionPosition);
    oversizedVersion.replace(
        versionPosition, std::strlen("\"schema_version\":1"),
        "\"schema_version\":4294967297");
    writeFile(profilesFile, oversizedVersion, 0444);
    expectRedisAuthError(
        [&] { RedisAuthConfig::fromProfile("local", profilesFile); },
        "Redis authentication profile file failed validation",
        {WRITER_CREDENTIAL});

    std::string oversizedPort = profiles.str();
    std::string validPort = "\"port\":" + std::to_string(m_server->port());
    size_t portPosition = oversizedPort.find(validPort);
    ASSERT_NE(std::string::npos, portPosition);
    uint64_t wrappedPort = (static_cast<uint64_t>(1) << 32) +
                           static_cast<uint64_t>(m_server->port());
    oversizedPort.replace(
        portPosition, validPort.size(),
        "\"port\":" + std::to_string(wrappedPort));
    writeFile(profilesFile, oversizedPort, 0444);
    expectRedisAuthError(
        [&] { RedisAuthConfig::fromProfile("local", profilesFile); },
        "Redis authentication profile file failed validation",
        {WRITER_CREDENTIAL});

    std::string invalidUser = profiles.str();
    size_t userPosition = invalidUser.find(WRITER_USER);
    ASSERT_NE(std::string::npos, userPosition);
    invalidUser.replace(userPosition, std::strlen(WRITER_USER), "invalid user");
    writeFile(profilesFile, invalidUser, 0444);
    expectRedisAuthError(
        [&] { RedisAuthConfig::fromProfile("local", profilesFile); },
        "Redis authentication profile file failed validation",
        {WRITER_CREDENTIAL});
}

}
