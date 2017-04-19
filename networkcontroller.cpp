#include "networkcontroller.h"
#include "fileio.h"
// Cross-platform development, here we come...
#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#include <winsock2.h>
#include <ws2ipdef.h>
#include <WS2tcpip.h>
typedef int socklen_t;
typedef SOCKET NETSOCK;
inline bool IsSocketValid(NETSOCK sock) { return sock != INVALID_SOCKET; }
#elif defined __linux__
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
#include <cstring>
#include <sys/types.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <netdb.h>
typedef int NETSOCK;
#define INVALID_SOCKET (-1)
#define SD_BOTH SHUT_RDWR
#define SD_SEND SHUT_WR
#define closesocket close
inline bool IsSocketValid(NETSOCK sock) { return sock >= 0; }
// remove this if linux ever implements memcpy_s
#define memcpy_s(dest, destsz, src, srcsz) memcpy(dest, src, destsz)
#endif
#include <future>
#include <iostream>
#include <sstream>


#if defined(_WIN32) || defined(_WIN64)
#define ReportError(Msg) std::cerr << WSAGetLastError() << " at " << __FUNCTION__ << Msg << std::endl;
#elif defined __linux__
#define ReportError(Msg) std::cerr << strerror(errno) << " at " << __FUNCTION__ << Msg << std::endl
#endif

std::unique_ptr<char[]> NetworkController::ReceivePrime(NETSOCK Socket, size_t Size)
{
    NETSOCK Result;
    size_t ReceivedData = 0;

    // yep, I'm allocating memory based on what a remote host tells me.
    // This is **VERY** INSECURE. Don't do it on networks that aren't secure.
    // We have very weak "authentication": only clients that have registered
    // with this server are allowed to send data.
    std::unique_ptr<char[]> Data(new char[Size]);

    do {
        Result = recv(Socket, Data.get() + ReceivedData, Size - ReceivedData, 0);
        if (!IsSocketValid(Result) || Result == 0)
        {
            ReportError(" failed recv");
            return nullptr;
        }
        ReceivedData += Result;

    } while (IsSocketValid(Result) && Result > 0 && ReceivedData < Size);

    return std::move(Data);
}

bool NetworkController::SendPrime(NETSOCK Socket, const char * const Prime, size_t Size)
{
    size_t SentData = 0;
    NETSOCK Result;

    do {
        Result = send(Socket, Prime + SentData, Size - SentData, 0);

        if (!IsSocketValid(Result))
        {
            ReportError(" send work");
            return false;
        }
        SentData += Result;

    } while (IsSocketValid(Result) && SentData < Size);

    return true;
}

std::string NetworkController::GetPrimeFileName(NetworkClientInfo & ClientInfo)
{
    AllPrimebotSettings Dummy;

    Dummy.FileSettings.Path = Settings.FileSettings.Path;
    Dummy.PrimeSettings.RngSeed = ClientInfo.Seed;
    Dummy.PrimeSettings.Bitsize = ClientInfo.Bitsize;

    if (Settings.FileSettings.Flags.Binary)
    {
        return ::GetPrimeFileNameBinary(Dummy, ClientInfo.RandomInteration);
    }
    else
    {
        return ::GetPrimeFileName(Dummy, ClientInfo.RandomInteration);
    }
}

void NetworkController::HandleRequest(NetworkConnectionInfo ClientSock)
{
    if (!IsSocketValid(ClientSock.ClientSocket))
    {
        return;
    }
    NetworkHeader Header = { 0 };
    int Result = 0;

    Result = recv(ClientSock.ClientSocket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" recv header");
        return;
    }

    Header.Size = ntohl(Header.Size);

    auto Client = Clients.find(ClientSock.addr.IPv6);

    switch (Header.Type)
    {
    case NetworkMessageType::RegisterClient:
        if (Clients.find(ClientSock.addr.IPv6) == Clients.end())
        {
            HandleRegisterClient(ClientSock);
        }
        else
        {
            ReportError(" Client tried to register more than once");
        }
        break;
    case NetworkMessageType::RequestWork:
        if (Client != Clients.end())
        {
            HandleRequestWork(ClientSock, Client->second);
        }
        break;
    case NetworkMessageType::ReportWork:
        if (Client != Clients.end())
        {
            HandleReportWork(ClientSock, Header.Size, Client->second);
        }
        break;
    case NetworkMessageType::BatchReportWork:
        if (Client != Clients.end())
        {
            HandleBatchReportWork(ClientSock, Header.Size, Client->second);
        }
        break;
    case NetworkMessageType::UnregisterClient:
        if (Client != Clients.end())
        {
            HandleUnregisterClient(ClientSock);
        }
        break;
    case NetworkMessageType::ShutdownClient:
        if (Settings.NetworkSettings.Client)
        {
            HandleShutdownClient(ClientSock);
        }
        break;
    default:
        return;
    }
    
    if (Settings.NetworkSettings.Server)
    {
        // Queue the client socket to be shutdown and closed asynchronously
        CompleteConnections.EnqueueWorkItem(std::move(ClientSock));
    }
    else if (Settings.NetworkSettings.Client)
    {
        // When finished, close the socket
        shutdown(ClientSock.ClientSocket, SD_BOTH);
        closesocket(ClientSock.ClientSocket);
    }
}

void NetworkController::CleanupRequest(NetworkConnectionInfo ClientSock)
{
    // When finished, close the client socket
    shutdown(ClientSock.ClientSocket, SD_BOTH);
    closesocket(ClientSock.ClientSocket);
}

bool NetworkController::RegisterClient()
{
    NetworkHeader Header = { 0 };
    NETSOCK Socket = GetSocketToServer();

    Header.Type = NetworkMessageType::RegisterClient;
    if (!IsSocketValid(send(Socket, (char*)&Header, sizeof(Header), 0)))
    {
        ReportError("");
        return false;
    }

    shutdown(Socket, SD_SEND);
    closesocket(Socket);
    return true;
}

void NetworkController::HandleRegisterClient(NetworkConnectionInfo& ClientSock)
{
    // This should also copy the IPv4 data if it's only IPv4.
    Clients.insert(std::make_pair<AddressType, NetworkClientInfo>(ClientSock.addr.IPv6, {}));

    std::cout << "Registered client: " << ClientSock.addr.ToString() << std::endl;
}

mpz_class NetworkController::RequestWork()
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    NetworkHeader Header = { 0 };
    Header.Type = NetworkMessageType::RequestWork;

    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        closesocket(Socket);
        return mpz_class();
    }

    shutdown(Socket, SD_SEND);

    Result = recv(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result) || Result == 0)
    {
        ReportError(" recv header");
        closesocket(Socket);
        return mpz_class();
    }

    Header.Size = ntohl(Header.Size);

    if (Header.Size < 1 || Header.Size > (INT_MAX >> 1))
    {
        ReportError(" header size invalid");
        closesocket(Socket);
        return mpz_class();
    }

    std::unique_ptr<char[]> Data(ReceivePrime(Socket, Header.Size));

    // Initialize the Workitem with the string returned from the server.
    mpz_class WorkItem(Data.get(), STRING_BASE);
    std::cout << "bit-length of start number: " << mpz_sizeinbase(WorkItem.get_mpz_t(), 2) << std::endl;

    closesocket(Socket);
    return std::move(WorkItem);
}

void NetworkController::HandleRequestWork(NetworkConnectionInfo& ClientSock, NetworkClientInfo& ClientInfo)
{
    NetworkHeader Header = { 0 };
    size_t Size;
    NETSOCK Result;

    // Generate prime number to send to client
    auto RandomInfo = Primebot::GenerateRandomOdd(Settings.PrimeSettings.Bitsize, Settings.PrimeSettings.RngSeed);
    mpz_class Work(RandomInfo.first);

    ClientInfo.RandomInteration = RandomInfo.second;
    ClientInfo.Bitsize = Settings.PrimeSettings.Bitsize;
    ClientInfo.Seed = Settings.PrimeSettings.RngSeed;

    Size = mpz_sizeinbase(Work.get_mpz_t(), STRING_BASE) + 2;

    Header.Type = NetworkMessageType::WorkItem;
    Header.Size = htonl((unsigned int) Size);

    // Tell client how large data is
    Result = send(ClientSock.ClientSocket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        return;
    }

    // send number back to requestor
    std::string Data = Work.get_str(STRING_BASE);

    // Send data now
    SendPrime(ClientSock.ClientSocket, Data.c_str(), Size);
}

bool NetworkController::ReportWork(mpz_class& WorkItem)
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    bool Success;
    size_t Size = mpz_sizeinbase(WorkItem.get_mpz_t(), STRING_BASE) + 2;
    NetworkHeader Header = { 0 };
    Header.Type = NetworkMessageType::ReportWork;
    Header.Size = htonl((unsigned int) Size);

    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        shutdown(Socket, SD_SEND);
        closesocket(Socket);
        return false;
    }

    std::string Data = WorkItem.get_str(STRING_BASE);

    // Send data now
    Success = SendPrime(Socket, Data.c_str(), Size);

    shutdown(Socket, SD_SEND);
    closesocket(Socket);
    return Success;
}

void NetworkController::HandleReportWork(NetworkConnectionInfo& ClientSock, int Size, NetworkClientInfo& ClientInfo)
{
    // Not sending any data on this socket, so shutdown send
    shutdown(ClientSock.ClientSocket, SD_SEND);

    if (Size < 0 || Size > (INT_MAX >> 1))
    {
        std::cerr << __FUNCTION__ << " failed size requirements" << std::endl;
        return;
    }

    std::unique_ptr<char[]> Data(ReceivePrime(ClientSock.ClientSocket, Size));

    if (Data == nullptr)
    {
        ReportError(" failed to recv workitem");
        return;
    }

    if (!Settings.FileSettings.Path.empty())
    {
        // write Data to file
        PendingIo.EnqueueWorkItem({ GetPrimeFileName(ClientInfo), std::move(Data) });
    }
    else
    {
        PendingIo.EnqueueWorkItem({ "", std::move(Data) });
    }
}

bool NetworkController::BatchReportWork(std::vector<mpz_class>& WorkItems)
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    NetworkHeader Header = { 0 };
    int Size;
    int NetSize;
    std::string Data;

    Header.Type = NetworkMessageType::BatchReportWork;
    Header.Size = htonl((unsigned int) WorkItems.size());

    // Send header with count of WorkItems
    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        shutdown(Socket, SD_SEND);
        closesocket(Socket);
        return false;
    }

    for (mpz_class& WorkItem : WorkItems)
    {
        Size = mpz_sizeinbase(WorkItem.get_mpz_t(), STRING_BASE) + 2;
        Data = WorkItem.get_str(STRING_BASE); // suspect this leaks memory

        // Convert Size to network-format.
        NetSize = htonl(Size);

        Result = send(Socket, (char*)&NetSize, sizeof(NetSize), 0);
        if (!IsSocketValid(Result))
        {
            ReportError(" send size");
            shutdown(Socket, SD_SEND);
            closesocket(Socket);
            return false;
        }

        if (!SendPrime(Socket, Data.c_str(), Size))
        {
            ReportError(" send prime");
            shutdown(Socket, SD_SEND);
            closesocket(Socket);
            return false;
        }
    }

    closesocket(Socket);
    return true;
}

void NetworkController::HandleBatchReportWork(NetworkConnectionInfo& ClientSock, int Count, NetworkClientInfo& ClientInfo)
{
    NETSOCK Result;
    int Size;
    std::unique_ptr<char[]> Data;
    std::vector<std::string> BatchData;

    BatchData.reserve(Count);

    // Only receiving data here
    shutdown(ClientSock.ClientSocket, SD_SEND);

    for (int i = 0; i < Count; i++)
    {
        // Receive size of prime
        Result = recv(ClientSock.ClientSocket, (char*)&Size, sizeof(Size), 0);
        if (!IsSocketValid(Result))
        {
            ReportError(" recv size");
            return;
        }

        Size = ntohl(Size);

        Data = ReceivePrime(ClientSock.ClientSocket, Size);

        if (Data.get() == nullptr)
        {
            ReportError(" recv prime " + std::to_string(i));
            return;
        }

        BatchData.emplace_back(Data.get());
    }

    // Enqueue work to a threadpool to actually handle the file IO, since
    // that may be slow. The server should have enough memory to hold all
    // the data until it is written out.
    if (!Settings.FileSettings.Path.empty())
    {
        // write Data to file
        PendingIo.EnqueueWorkItem({ GetPrimeFileName(ClientInfo), nullptr, std::move(BatchData) });
    }
    else
    {
        PendingIo.EnqueueWorkItem({ "", nullptr, std::move(BatchData) });
    }
}

void NetworkController::UnregisterClient()
{
    NetworkHeader Header = { NetworkMessageType::UnregisterClient, 0 };
    NETSOCK Socket = GetSocketToServer();

    send(Socket, (char*)&Header, sizeof(Header), 0);
    // wait for response from server
    recv(Socket, (char*)&Header, sizeof(Header), 0);
    closesocket(Socket);
}

void NetworkController::HandleUnregisterClient(NetworkConnectionInfo& ClientSock)
{
    NetworkHeader Header = { 0 };
    Header.Type = NetworkMessageType::ShutdownClient;

    if(!IsSocketValid(send(ClientSock.ClientSocket, (char*)&Header, sizeof(Header), 0)))
    {
        ReportError(" send unregister response to " + ClientSock.addr.ToString());
    }

    Clients.erase(ClientSock.addr);

    std::cout << "Unregistered client: " << ClientSock.addr.ToString() << std::endl;
}

void NetworkController::ShutdownClients()
{
    NetworkHeader Header = { NetworkMessageType::ShutdownClient, 0 };
    NETSOCK Result;

    // for each client
    for (auto client : Clients)
    {
        AddressType addr = client.first;
        // add back the client's port
        if (addr.IPv4.sin_family == AF_INET)
        {
            addr.IPv4.sin_port = CLIENT_PORT;
        }
        else
        {
            addr.IPv6.sin6_port = CLIENT_PORT;
        }

        // open a socket to the client
        NETSOCK Socket = GetSocketTo(addr.IPv6);

        // send shutdown message
        Result = send(Socket, (char*)&Header, sizeof(Header), 0);
        if (!IsSocketValid(Result))
        {
            ReportError(" send shutdown failed for " + addr.ToString());
        }

        shutdown(Socket, SD_BOTH);
        closesocket(Socket);
    }
}

void NetworkController::HandleShutdownClient(NetworkConnectionInfo& ServerSock)
{
    if (ServerSock.addr == Settings.NetworkSettings.IPv6)
    {
        std::cout << "Server instructed client to shutdown" << std::endl;
        Bot->Stop();
    }
}

void NetworkController::ProcessIO(ControllerIoInfo Info)
{
    // Single-item case
    if (Info.Data != nullptr)
    {
        if (Info.Name.empty())
        {
            mpz_class Work(Info.Data.get(), STRING_BASE);

            //std::cout << Work << std::endl; // linker error on windows
            gmp_fprintf(stdout, "%Zd\n", Work.get_mpz_t());
        }
        else
        {
            if (Settings.FileSettings.Flags.Binary)
            {
                WritePrimeToSingleFileBinary(Settings.FileSettings.Path, Info.Name, Info.Data.get());
            }
            else
            {
                WritePrimeToSingleFile(Settings.FileSettings.Path, Info.Name, Info.Data.get());
            }
        }
    }
    else // batch-work case
    {
        if (Info.Name.empty())
        {
            for (std::string & d : Info.BatchData)
            {
                mpz_class Work(d, STRING_BASE);

                //std::cout << Work << std::endl; // linker error on windows
                gmp_fprintf(stdout, "%Zd\n", Work.get_mpz_t());
            }
        }
        else
        {
            if (Settings.FileSettings.Flags.Binary)
            {
                WritePrimesToSingleFileBinary(Settings.FileSettings.Path, Info.Name, Info.BatchData);
            }
            else
            {
                WritePrimesToSingleFile(Settings.FileSettings.Path, Info.Name, Info.BatchData);
            }
        }
    }

}

void NetworkController::ListenLoop()
{
    while (true)
    {
        NetworkConnectionInfo ConnInfo;
        //memset(ConnInfo.get(), 0, sizeof(NetworkConnectionInfo));
        
        socklen_t ConnInfoAddrSize = sizeof(ConnInfo.addr.IPv6);

        ConnInfo.ClientSocket = accept(ListenSocket, (sockaddr*)&ConnInfo.addr.IPv6, &ConnInfoAddrSize);

        if (!IsSocketValid(ConnInfo.ClientSocket))
        {
            ReportError(" accepting connection failed");
            continue;
        }

        if (Settings.NetworkSettings.Server)
        {
            //std::thread(&NetworkController::HandleRequest, this, std::move(ConnInfo)).detach(); // potentially thousands of threads
            OutstandingConnections.EnqueueWorkItem(std::move(ConnInfo));
        }
        else if (Settings.NetworkSettings.Client)
        {
            // process in-line
            HandleRequest(ConnInfo);
        }
    }
}

void NetworkController::ClientBind()
{
    AddressType ClientAny;

    // match IP family and port number of server, because I'm lazy
    if (Settings.NetworkSettings.IPv4.sin_family == AF_INET)
    {
        ClientAny.IPv4.sin_family = AF_INET;
        ClientAny.IPv4.sin_port = CLIENT_PORT;//Settings.IPv4.sin_port;
        if (!IsSocketValid(
                bind(
                    ListenSocket,
                    (sockaddr*)&ClientAny.IPv4,
                    sizeof(ClientAny.IPv4))))
        {
            ReportError(" bind IPv4");
        }
    }
    else
    {
        ClientAny.IPv6.sin6_family = AF_INET6;
        ClientAny.IPv6.sin6_port = CLIENT_PORT;//Settings.IPv6.sin6_port;

        if (!IsSocketValid(
                bind(
                    ListenSocket,
                    (sockaddr*)&ClientAny.IPv6,
                    sizeof(ClientAny.IPv6))))
        {
            ReportError("bind IPv6");
        }
    }
}

void NetworkController::ServerBind()
{
    if (Settings.NetworkSettings.IPv4.sin_family == AF_INET)
    {
        if (!IsSocketValid(
                bind(
                    ListenSocket,
                    (sockaddr*)&Settings.NetworkSettings.IPv4,
                    sizeof(Settings.NetworkSettings.IPv4))))
        {
            ReportError(" bind IPv4");
        }
    }
    else
    {
        if (!IsSocketValid(
                bind(
                    ListenSocket,
                    (sockaddr*)&Settings.NetworkSettings.IPv6,
                    sizeof(Settings.NetworkSettings.IPv6))))
        {
            ReportError(" bind IPv6");
        }
    }
}

NETSOCK NetworkController::GetSocketTo(sockaddr_in6& Client)
{
    NETSOCK Result;
    int Enable = 1;
    NETSOCK Socket = socket(Client.sin6_family, SOCK_STREAM, IPPROTO_TCP);
    if (!IsSocketValid(Socket))
    {
        ReportError(" socket creation");
    }

    setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, (char*)&Enable, sizeof(Enable));

    if (Client.sin6_family == AF_INET)
    {
        Result =
            connect(
                Socket,
                (sockaddr*)&Client,
                sizeof(sockaddr_in));
        if (!IsSocketValid(Result))
        {
            ReportError(" connect ipv4");
            closesocket(Socket);
        }
    }
    else
    {
        Result =
            connect(
                Socket,
                (sockaddr*)&Client,
                sizeof(Client));
        if (!IsSocketValid(Result))
        {
            ReportError(" connect IPv6");
            closesocket(Socket);
        }
    }

    return Socket;
}

NETSOCK NetworkController::GetSocketToServer()
{
    return GetSocketTo(Settings.NetworkSettings.IPv6);
}

NetworkController::NetworkController(AllPrimebotSettings Config) :
    Settings(Config),
    OutstandingConnections(
        std::thread::hardware_concurrency(),
        std::bind(&NetworkController::HandleRequest, this, std::placeholders::_1)),
    CompleteConnections(
        std::thread::hardware_concurrency(),
        std::bind(&NetworkController::CleanupRequest, this, std::placeholders::_1)),
    PendingIo(
        1, // TODO: make IO Threads configurable
        std::bind(&NetworkController::ProcessIO, this, std::placeholders::_1)),
    ListenSocket(INVALID_SOCKET),
    Bot(nullptr)
{
#if defined(_WIN32) || defined(_WIN64)
    WSAData WsData;
    if (WSAStartup(MAKEWORD(2, 2), &WsData) != NO_ERROR)
    {
        ReportError("");
    }
#endif

    if (!Settings.NetworkSettings.Server)
    {
        // Threadpools unused on client, so stop them to free resources
        OutstandingConnections.Stop();
        CompleteConnections.Stop();
        PendingIo.Stop();
    }
}

NetworkController::~NetworkController()
{
    if (IsSocketValid(ListenSocket))
    {
        shutdown(ListenSocket, SD_BOTH);
        closesocket(ListenSocket);
    }
#ifdef _WIN32
    WSACleanup();
#endif

    if (Settings.NetworkSettings.Server)
    {
        OutstandingConnections.Stop();
        CompleteConnections.Stop();
        PendingIo.Stop();
    }
}

void NetworkController::Start()
{
    int Result = 0;

    // Clients and servers both listen on a socket for incoming connections
    // Clients just need to make sure the connection is from the server.

    ListenSocket = socket(Settings.NetworkSettings.IPv4.sin_family, SOCK_STREAM, IPPROTO_TCP);
    if (!IsSocketValid(ListenSocket))
    {
        ReportError(" socket creation");
    }

    if (Settings.NetworkSettings.Server)
    {
        ServerBind();
    }
    else
    {
        ClientBind();
    }

    Result = listen(ListenSocket, SOMAXCONN);
    if (!IsSocketValid(Result))
    {
        ReportError(" listen");
    }
    
    // Actually start waiting for connections
    if (Settings.NetworkSettings.Server)
    {
        // This will never return
        ListenLoop();
    }
    else
    {
        // On the other hand, clients need to do other things, so run this
        // loop in a never-returning thread.
        std::thread(&NetworkController::ListenLoop, this).detach();
    }
}

void NetworkController::Shutdown()
{
    ShutdownClients();

    // wait for list of clients to go to zero, indicating all have unregistered.
    auto RemainingClients = Clients.size();
    auto RemainingIo = PendingIo.GetWorkItemCount();
    while (RemainingClients > 0 || RemainingIo > 0)
    {
        std::cout << "Waiting 1 second for remaining clients: " << RemainingClients
            << ", remaining I/Os: " << RemainingIo << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));
        RemainingClients = Clients.size();
        RemainingIo = PendingIo.GetWorkItemCount();
    }
}

// Included here to reduce the number of Winsock headers to drag into
// project headers.
inline bool operator<(const AddressType& Left, const AddressType& Right)
{
    // Compare address families
    if (Left.IPv4.sin_family < Right.IPv4.sin_family)
    {
        return true;
    }

    if (Left.IPv4.sin_family == AF_INET)
    {
        return (Left.IPv4.sin_addr.s_addr < Right.IPv4.sin_addr.s_addr);
    }

    return (memcmp(&Left.IPv6.sin6_addr, &Right.IPv6.sin6_addr, sizeof(sockaddr_in6)) < 0);
}

inline bool operator==(const AddressType & Left, const AddressType & Right)
{
    if (Left.IPv4.sin_family != Right.IPv4.sin_family)
    {
        return false;
    }

    if (Left.IPv4.sin_family == AF_INET)
    {
        return Left.IPv4.sin_addr.s_addr == Right.IPv4.sin_addr.s_addr;
    }

    // Assume IPv6

    return memcmp(&Left.IPv6.sin6_addr, &Right.IPv6.sin6_addr, sizeof(Left.IPv6.sin6_addr)) == 0;;
}

std::string AddressType::ToString()
{
    std::stringstream AddressString;
    if (IPv4.sin_family == AF_INET)
    {

        AddressString << std::hex << ntohl(IPv4.sin_addr.s_addr);
    }
    else
    {
        for (unsigned char* idx = std::begin(IPv6.sin6_addr.s6_addr);
            idx < std::end(IPv6.sin6_addr.s6_addr);
            idx++)
        {
            AddressString << std::hex << idx;
        }
    }
    AddressString << std::endl;

    return AddressString.str();
}
