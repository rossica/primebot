#pragma once

#include <memory>
#include <set>

// Cross-platform development, here we come...
#ifdef _WIN32
#include <winsock2.h>
#include <ws2ipdef.h>
typedef SOCKET NETSOCK;
inline bool IsSocketValid(NETSOCK sock) { return sock != INVALID_SOCKET; }
#elif defined __linux__
typedef int NETSOCK;
#define INVALID_SOCKET (-1)
inline bool IsSocketValid(NETSOCK sock) { return sock > 0; }
#endif

// Forward Declarations
class Primebot;

struct NetworkControllerSettings
{
    union
    {
        sockaddr_in IPv4;
        sockaddr_in6 IPv6;
    };

    // If true: this is server, listen on address.
    // If false: this is client, connect to address.
    bool Server;
};

struct NetworkConnectionInfo
{
    union
    {
        sockaddr_in IPv4;
        sockaddr_in6 IPv6;
    };
    NETSOCK ClientSocket;
};

struct NetworkHeader
{
    // Type of NetworkMessage
    char Type;
    // Size of Data section (doesn't include the header)
    // N.B. This will be Network byte-order until converted.
    int Size;
};

enum NetworkMessageType
{
    Invalid = 0,
    RegisterClient, // 0 size, no message
    ClientRegistered, // ACKs client registration
    RequestWork, // 0 size, no message
    WorkItem, // binary representation of workitem
    ReportWork, // size of workitem, binary representation of workitem
    WorkAccepted, // 0 size, no message
    UnregisterClient, // 0 size, sent by client during shutdown
    ShutdownClient // 0 size, can be server initiated message
};

union AddressType
{
    sockaddr_in IPv4;
    sockaddr_in6 IPv6;
    AddressType(sockaddr_in6& ip6)
    {
        IPv6 = ip6;
        if (IPv6.sin6_family == AF_INET6)
        {
            IPv6.sin6_port = 0;
            IPv6.sin6_flowinfo = 0;
        }
        else
        {
            IPv4.sin_port = 0;
            IPv6.sin6_addr = { 0 };
        }
    }
    AddressType(sockaddr_in& ip4)
    {
        IPv4 = ip4;
        IPv4.sin_port = 0;
    }
};

inline bool operator<(const AddressType& Left, const AddressType& Right)
{
    // Compare address families
    if (Left.IPv4.sin_family < Right.IPv4.sin_family)
    {
        return true;
    }

    if (Left.IPv4.sin_family == AF_INET)
    {
        return (Left.IPv4.sin_addr.S_un.S_addr < Right.IPv4.sin_addr.S_un.S_addr);
    }

    return (memcmp(&Left.IPv6.sin6_addr, &Right.IPv6.sin6_addr, sizeof(sockaddr_in6)) < 0);
}

class NetworkController
{
private:
    NetworkControllerSettings Settings;
    std::set<AddressType> Clients;
    NETSOCK ListenSocket;
    Primebot* Bot;

    // handles incoming requests, for client and server
    void NetworkController::HandleRequest(NetworkConnectionInfo ClientSock);

    void HandleRegisterClient(NetworkConnectionInfo& ClientSock);
    void HandleRequestWork(NetworkConnectionInfo& ClientSock);
    void HandleReportWork(NetworkConnectionInfo& ClientSock, int Size);
    void HandleUnregisterClient(NetworkConnectionInfo& ClientSock);
    void HandleShutdownClient(NetworkConnectionInfo& ServerSock);

    void CleanupRequest(NetworkConnectionInfo ClientSock);

    void ListenLoop();
    void ClientBind();
    void ServerBind();

    NETSOCK GetSocketTo(sockaddr_in6& Client);
    NETSOCK GetSocketToServer();
public:
    NetworkController() = delete;
    NetworkController(NetworkControllerSettings Config);
    ~NetworkController();
    void Start();

    void SetPrimebot(Primebot* bot) { Bot = bot; }

    void RegisterClient();
    int RequestWork();
    void ReportWork(int WorkItem);
    void UnregisterClient();
    void ShutdownClients();
};