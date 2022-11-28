#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <system_error>
#include "selectable.h"
#include "common/logger.h"
#include "hbclient.h"
#include <string>

using namespace swss;
using namespace std;

/* Message buffer for sending messages to HB monitor server */
hb_client_message_t message;
static struct sockaddr_un client_sock_addr, server_sock_addr;

HBClient::HBClient(std::string appName,
                   hb_client_sla_t sla) :
    Selectable(), m_appName(appName), m_socket(0), m_sla(sla)
{
    if ((m_socket = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
        SWSS_LOG_ERROR("Unable to create HB Client socket");
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to create HB Client socket");
    }

    m_pid = getpid();
    m_registerStatus = false;
    memset (&client_sock_addr, 0, sizeof(struct sockaddr_un));
    client_sock_addr.sun_family = AF_UNIX;
    sprintf (client_sock_addr.sun_path, "/tmp/hbclient-%s-%u", m_appName.c_str(), m_pid);

    /* Remove any previous path with the same name */
    unlink(client_sock_addr.sun_path);

    if (bind(m_socket, (const struct sockaddr *) &client_sock_addr,
             sizeof(struct sockaddr_un)) < 0)
    {
      SWSS_LOG_ERROR("Unable to bind HB Client Unix socket to path %s", client_sock_addr.sun_path);
      close (m_socket);
      m_socket = -1;
      return;
    }

    (void) chmod (client_sock_addr.sun_path, 0777);


    /* Register with the HB manager */
    memset (&server_sock_addr, 0, sizeof(struct sockaddr_un));
    server_sock_addr.sun_family = AF_UNIX;
    sprintf (server_sock_addr.sun_path, "/tmp/hbserver");

    memset (&message, 0, sizeof(message));
    /* Send Register message */
    message.version = HB_VERSION_1_0;
    strncpy(message.process_name, m_appName.c_str(), sizeof(message.process_name)-1);
    message.process_pid = getpid();
    /* Fill the Client SLA */
    message.sla = m_sla;

    registerWithServer();
}

HBClient::~HBClient()
{
    ssize_t ret;

    if (m_socket != -1)
    {
        /* Send Deregister message */
        message.msg_type = HB_CLIENT_DEREGISTER;
        do
        {
            ret = sendto(m_socket, (void *) &message, sizeof(message), 0,
                         (struct sockaddr *) &server_sock_addr, sizeof(server_sock_addr));
        }
        while ((ret < 0) && (errno == EINTR));
        close(m_socket);
    }
}

int HBClient::getFd()
{
    return m_socket;
}

bool HBClient::clientRegistered()
{
    return m_registerStatus;
}

void HBClient::registerWithServer()
{
    ssize_t ret;
    message.msg_type = HB_CLIENT_REGISTER;

    do
    {
      ret = sendto(m_socket, (void *) &message, sizeof(message), 0,
                   (struct sockaddr *) &server_sock_addr, sizeof(server_sock_addr));
    }
    while ((ret < 0) && (errno == EINTR));
    if (ret <= 0)
    {
        SWSS_LOG_ERROR("HB Client failed to send REGISTER to HB server with error - %d", errno);
    }
    else
    {
        m_registerStatus = true;
        SWSS_LOG_NOTICE("HB Client registered successfully with the HB server");
    }
}

uint64_t HBClient::readData()
{
    ssize_t ret, addr_size;
    hb_server_message_t srvrmsg;
 
    memset(&srvrmsg, 0, sizeof(srvrmsg));
    addr_size = sizeof(server_sock_addr);
    do
    {
        /* Receive HB messages from the server */
        ret = recvfrom(m_socket, &srvrmsg, sizeof(srvrmsg), 0,
                       (struct sockaddr *)&server_sock_addr,
                       (socklen_t *)&addr_size);
    }
    while ((ret < 0) && (errno == EINTR)); // Retry if the process was interrupted by a signal

    if (ret < 0)
    {
        SWSS_LOG_ERROR("HB Client failed to read server HB message with error - %d", errno);
    }
    else if (ret == sizeof(srvrmsg))
    {
        SWSS_LOG_DEBUG("HB Client received HB ECHO message from server");

        /* Reply back with an ACK */
        message.msg_type = HB_ECHO_ACK;
        do
        {
          ret = sendto(m_socket, (void *) &message, sizeof(message), 0,
                       (struct sockaddr *) &server_sock_addr, sizeof(server_sock_addr));
        }
        while ((ret < 0) && (errno == EINTR));
        
        if (ret < 0)
        {
            SWSS_LOG_ERROR("HB Client failed to send ACK to server HB message with error - %d", errno);
        }
        else if (ret == sizeof(message))
        {
            SWSS_LOG_DEBUG("HB Client replied back with an ACK to server HB message");
        }
    } 
    return 0;
}
