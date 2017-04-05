#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX // windows.h provides MIN/MAX macros that conflict with min()/max() in gmpxx.h
#include <winsock2.h>
#include <ws2ipdef.h>
typedef SOCKET NETSOCK;
#elif defined __linux__
#include <sys/types.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <netdb.h>
typedef int NETSOCK;
#define INVALID_SOCKET (-1)
#endif

#include <memory>
#include <set>
#include <list>
#include "threadpool.h"
#include "prime.h"
#include "commandparsertypes.h"

#define CLIENT_PORT (htons(60001))

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
    BatchReportWork, // count of (size,workitem) pairs
    BatchWorkAccepted, // 0 size, no message
    UnregisterClient, // 0 size, sent by client during shutdown
    ShutdownClient // 0 size, server initiated message
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

    AddressType()
    {
        IPv6 = { 0 };
    }
};

inline bool operator<(const AddressType& Left, const AddressType& Right);

class NetworkController
{
private:
    AllPrimebotSettings Settings;
    Threadpool<NetworkConnectionInfo, std::list<NetworkConnectionInfo>> tp;
    std::set<AddressType> Clients;
    NETSOCK ListenSocket;
    Primebot* Bot;

    // helper functions
    char* ReceivePrime(NETSOCK Socket, char* Data, int Size);
    bool SendPrime(NETSOCK Socket, const char const * Prime, int Size);

    // handles incoming requests, for client and server
    void HandleRequest(decltype(tp)& pool, NetworkConnectionInfo ClientSock);

    void HandleRegisterClient(NetworkConnectionInfo& ClientSock);
    void HandleRequestWork(NetworkConnectionInfo& ClientSock);
    void HandleReportWork(NetworkConnectionInfo& ClientSock, int Size);
    void HandleBatchReportWork(NetworkConnectionInfo& ClientSock, int Count);
    void HandleUnregisterClient(NetworkConnectionInfo& ClientSock);
    void HandleShutdownClient(NetworkConnectionInfo& ServerSock);

    void CleanupRequest(decltype(tp)& pool, NetworkConnectionInfo ClientSock);

    void ListenLoop();
    void ClientBind();
    void ServerBind();

    NETSOCK GetSocketTo(sockaddr_in6& Client);
    NETSOCK GetSocketToServer();
public:
    NetworkController() = delete;
    NetworkController(AllPrimebotSettings Config);
    ~NetworkController();
    void Start();

    void SetPrimebot(Primebot* bot) { Bot = bot; }

    bool RegisterClient();
    unique_mpz RequestWork();
    bool ReportWork(__mpz_struct& WorkItem);
    bool BatchReportWork(std::vector<unique_mpz>& WorkItems);
    bool BatchReportWork(std::vector<mpz_class>& WorkItems);
    void UnregisterClient();
    void ShutdownClients();
};