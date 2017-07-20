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
#include <map>
#include <set>
#include <list>
#include <chrono>
#include <random>
#include <mutex>
#include <ratio>
#include "threadpool.h"
#include "prime.h"
#include "fileio.h"
#include "commandparsertypes.h"

#define CLIENT_PORT (htons(60001))
#define STRING_BASE (62)
#define INVALID_ID 0

///////////////////////////////////////////////////
//
//  Types for communicating with other modules
//
///////////////////////////////////////////////////

struct ClientWorkitem
{
    uint64_t Id;
    mpz_class Start;
    uint32_t Offset;
};

///////////////////////////////////////
//
//  On-Wire enumerations and types
//
///////////////////////////////////////

struct NetworkHeader
{
    // Type of NetworkMessage
    uint8_t Type;
    // Size of Data section (doesn't include the header)
    // N.B. This will be Network byte-order until converted.
    int32_t Size;
};

enum NetworkMessageType
{
    Invalid = 0,
    RegisterClient, // 0 size, no message
    ClientRegistered, // ACKs client registration
    RequestWork, // size = 2, # of workitems requested
    WorkItem, // ID, Offset, binary representation of workitem
    ReportWork, // size = count of (size,workitem) pairs
    WorkAccepted, // 0 size, no message; un-implemented
    UnregisterClient, // 0 size, sent by client during shutdown
    ShutdownClient // 0 size, server initiated message
};

struct RequestWorkMessage
{
    // Requested count of workitems, in network byte-order
    uint16_t WorkitemCount;
};

struct WorkItemMessage
{
    // Work item Id, in network byte-order
    uint64_t Id;
    // Offset to search until, in network byte-order
    uint32_t Offset;
    char Workitem[];
};

struct OnWireWorkItem
{
    // THe number of bytes in Workitem, in network byte-order
    uint32_t Size;
    // The string representation of the number
    char Workitem[];
};

struct ReportWorkMessage
{
    // Work item Id, in network byte-order
    uint64_t Id;
    // There's more than one of these; this is just the first one.
    OnWireWorkItem WorkItem;
};

////////////////////////////////
//
//  Internal bookkeeping types
//
////////////////////////////////

struct AddressType
{
    union
    {
        sockaddr_in IPv4;
        sockaddr_in6 IPv6;
    };

    AddressType(const sockaddr_in6& ip6)
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

    AddressType(const sockaddr_in& ip4)
    {
        IPv4 = ip4;
        IPv4.sin_port = 0;
    }

    AddressType()
    {
        IPv6 = { 0 };
    }

    std::string ToString();
};

inline bool operator<(const AddressType& Left, const AddressType& Right);
inline bool operator==(const AddressType& Left, const AddressType& Right);


// forward decl
struct NetworkClientInfo;

class NetworkPendingWorkitem
{
private:
    static std::random_device Rd;
    static std::seed_seq Seed;
    static std::mt19937_64 Rng;
    static std::uniform_int_distribution<uint64_t> Distribution;
    static uint64_t Key;
public:
    uint64_t Id;
    mpz_class Start;
    uint32_t Range;
    NetworkPendingWorkitem* Next;
    std::shared_ptr<NetworkClientInfo> AssignedClient;
    std::shared_ptr<NetworkClientInfo> ReportingClient;
    std::chrono::steady_clock::time_point SendTime;
    std::chrono::steady_clock::time_point ReceivedTime;
    std::vector<std::unique_ptr<char[]>> Data;
    bool DataReceived;

    explicit NetworkPendingWorkitem() = default;
    NetworkPendingWorkitem(mpz_class Value, uint32_t Offset, std::shared_ptr<NetworkClientInfo> Cli);
    NetworkPendingWorkitem(const NetworkPendingWorkitem&) = delete;
    NetworkPendingWorkitem(NetworkPendingWorkitem&&) = default;
    NetworkPendingWorkitem CopyWork();
};

struct NetworkConnectionInfo
{
    AddressType addr;
    NETSOCK ClientSocket;
};

struct NetworkClientInfo
{
    typedef std::ratio<1,2> MeanWeight;
    uint64_t TotalAssignedWorkitems;
    uint64_t TotalCompletedWorkitems;
    std::chrono::steady_clock::duration AvgCompletionTime;
    std::chrono::steady_clock::time_point LastReceivedTime;
    AddressType Key; // yes this wastes some space
    bool Zombie;
};

class NetworkController
{
private:
    typedef std::ratio<1,2> MeanWeight;
    const AllPrimebotSettings& Settings;
    PrimeFileIo FileIo;
    Threadpool<NetworkConnectionInfo, std::list<NetworkConnectionInfo>> OutstandingConnections;
    std::map<AddressType, std::shared_ptr<NetworkClientInfo>> Clients;
    NETSOCK ListenSocket;
    Primebot* Bot;
    mpz_class InitialValue;
    mpz_class NextWorkitem;
    std::map<uint64_t, NetworkPendingWorkitem> PendingWorkitems;
    NetworkPendingWorkitem* FirstPendingWorkitem;
    NetworkPendingWorkitem* LastPendingWorkitem;
    std::mutex PendingWorkitemsLock;
    std::chrono::steady_clock::duration AvgClientCompletionTime;
    std::condition_variable_any IoWaitVariable;
    std::mutex IoWaitLock;
    uint64_t NewAssignments;
    uint64_t DuplicateAssignments;
    uint64_t DuplicateReceives;
    uint64_t MissingReceives;

    // helper functions
    std::unique_ptr<char[]> ReceivePrime(NETSOCK Socket, size_t Size);
    bool SendPrime(NETSOCK Socket, const char * const Prime, size_t Size);
    uint64_t ReceiveId(NETSOCK Socket);
    bool SendId(NETSOCK Socket, uint64_t Id);
    uint32_t ReceiveOffset(NETSOCK Socket);
    bool SendOffset(NETSOCK Socket, uint32_t Offset);
    NetworkPendingWorkitem CreateWorkitem(uint16_t WorkitemCount, std::shared_ptr<NetworkClientInfo> Client);
    bool CompleteWorkitem(uint64_t Id, std::shared_ptr<NetworkClientInfo> Client, std::vector<std::unique_ptr<char[]>> ReceivedData);
    NetworkPendingWorkitem RemoveWorkitem();
    int LivingClientsCount();

    // handles incoming requests, for client and server
    void HandleRequest(NetworkConnectionInfo&& ClientSock);

    void HandleRegisterClient(NetworkConnectionInfo& ClientSock);
    void HandleRequestWork(NetworkConnectionInfo& ClientSock, std::shared_ptr<NetworkClientInfo> ClientInfo);
    void HandleReportWork(NetworkConnectionInfo& ClientSock, int Count, std::shared_ptr<NetworkClientInfo> ClientInfo);
    void HandleUnregisterClient(NetworkConnectionInfo& ClientSock);
    void HandleShutdownClient(NetworkConnectionInfo& ServerSock);

    // Handle IO
    void IoLoop();

    void ListenLoop();
    void ClientBind();
    void ServerBind();

    NETSOCK GetSocketTo(const sockaddr_in6& Client);
    NETSOCK GetSocketToServer();
public:
    NetworkController() = delete;
    NetworkController(const AllPrimebotSettings& Config);
    ~NetworkController();
    void Start();
    void Shutdown();

    void SetPrimebot(Primebot* bot) { Bot = bot; }

    bool RegisterClient();
    ClientWorkitem RequestWork(uint16_t Count);
    bool ReportWork(uint64_t Id, const std::vector<mpz_class>& WorkItems);
    void UnregisterClient();
    void ShutdownClients();
};