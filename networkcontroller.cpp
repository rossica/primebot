#include "networkcontroller.h"
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
#define memcpy_s(dest, destsz, src, srcsz) memcpy(dest, src, destsz)
#endif
#include <future>
#include <iostream>


#if defined(_WIN32) || defined(_WIN64)
#define ReportError(Msg) std::cerr << WSAGetLastError() << " at " << __FUNCTION__ << Msg << std::endl;
#elif defined __linux__
#define ReportError(Msg) std::cerr << strerror(errno) << " at " << __FUNCTION__ << Msg << std::endl
#endif

#define STRING_BASE (62)

char* NetworkController::ReceivePrime(NETSOCK Socket, char* Data, int Size)
{
    NETSOCK Result;
    int ReceivedData = 0;
    std::unique_ptr<char[]> TempData(nullptr);

    // yep, I'm allocating memory based on what a remote host tells me.
    // This is **VERY** INSECURE. Don't do it on networks that aren't secure.
    // We have very weak "authentication": only clients that have registered
    // with this server are allowed to send data.
    if (Data == nullptr)
    {
        // Storing array in a unique_ptr to prevent leaks if this function crashes.
        TempData.reset(new char[Size]);
        Data = TempData.get();
    }

    do {
        Result = recv(Socket, Data + ReceivedData, Size - ReceivedData, 0);
        if (!IsSocketValid(Result) || Result == 0)
        {
            ReportError(" failed recv");
            return nullptr;
        }
        ReceivedData += Result;

    } while (IsSocketValid(Result) && Result > 0 && ReceivedData < Size);

    return (TempData.get() != nullptr) ? TempData.release() : Data;
}

bool NetworkController::SendPrime(NETSOCK Socket, const char const * Prime, int Size)
{
    int SentData = 0;
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

void NetworkController::HandleRequest(decltype(OutstandingConnections)& pool, NetworkConnectionInfo ClientSock)
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

    switch (Header.Type)
    {
    case NetworkMessageType::RegisterClient:
        HandleRegisterClient(ClientSock);
        break;
    case NetworkMessageType::RequestWork:
        if (Clients.find(ClientSock.IPv6) != Clients.end())
        {
            HandleRequestWork(ClientSock);
        }
        break;
    case NetworkMessageType::ReportWork:
        if (Clients.find(ClientSock.IPv6) != Clients.end())
        {
            HandleReportWork(ClientSock, Header.Size);
        }
        break;
    case NetworkMessageType::BatchReportWork:
        if (Clients.find(ClientSock.IPv6) != Clients.end())
        {
            HandleBatchReportWork(ClientSock, Header.Size);
        }
        break;
    case NetworkMessageType::UnregisterClient:
        if (Clients.find(ClientSock.IPv6) != Clients.end())
        {
            HandleUnregisterClient(ClientSock);
        }
        break;
    case NetworkMessageType::ShutdownClient:
        if (!Settings.NetworkSettings.Server)
        {
            HandleShutdownClient(ClientSock);
        }
        break;
    default:
        return;
    }
    
    //// When finished, close the client socket
    shutdown(ClientSock.ClientSocket, SD_BOTH);
    CompleteConnections.EnqueueWorkItem(std::move(ClientSock));
}

void NetworkController::CleanupRequest(decltype(CompleteConnections)& pool, NetworkConnectionInfo ClientSock)
{
    // When finished, close the client socket
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
    Clients.insert(ClientSock.IPv6);

    std::cout << "Registered client: ";
    if (ClientSock.IPv4.sin_family == AF_INET)
    {
        std::cout << std::hex << ntohl(ClientSock.IPv4.sin_addr.s_addr);
    }
    else
    {
        for (unsigned char* idx = std::begin(ClientSock.IPv6.sin6_addr.u.Byte);
            idx < std::end(ClientSock.IPv6.sin6_addr.u.Byte);
            idx++)
        {
            std::cout << std::hex << idx;
        }
    }
    std::cout << std::endl;
}

unique_mpz NetworkController::RequestWork()
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    int ReceivedData = 0;
    NetworkHeader Header = { 0 };
    Header.Type = NetworkMessageType::RequestWork;

    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        closesocket(Socket);
        return nullptr;
    }

    shutdown(Socket, SD_SEND);

    Result = recv(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result) || Result == 0)
    {
        ReportError(" recv header");
        closesocket(Socket);
        return nullptr;
    }

    Header.Size = ntohl(Header.Size);

    if (Header.Size < 1 || Header.Size > (INT_MAX >> 1))
    {
        ReportError(" header size invalid");
        closesocket(Socket);
        return nullptr;
    }

    std::unique_ptr<char[]> Data(ReceivePrime(Socket, nullptr, Header.Size));

    // Initialize the Workitem with the string returned from the server.
    unique_mpz WorkItem(new __mpz_struct);
    mpz_init_set_str(WorkItem.get(), Data.get(), STRING_BASE);
    std::cout << "bit-length of start number: " << mpz_sizeinbase(WorkItem.get(), 2) << std::endl;

    closesocket(Socket);
    return std::move(WorkItem);
}

void NetworkController::HandleRequestWork(NetworkConnectionInfo& ClientSock)
{
    NetworkHeader Header = { 0 };
    size_t Size;
    int SentData = 0;
    NETSOCK Result;

    // Generate prime number to send to client
    unique_mpz Work(Primebot::GenerateRandomOdd(Settings.PrimeSettings.Bitsize, Settings.PrimeSettings.RngSeed));

    Size = mpz_sizeinbase(Work.get(), STRING_BASE) + 2;

    Header.Type = NetworkMessageType::WorkItem;
    Header.Size = htonl(Size);

    // Tell client how large data is
    Result = send(ClientSock.ClientSocket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        return;
    }

    // send number back to requestor
    // Using this instead of mpz_class::get_str because that
    // appears to have a memory leak.
    std::unique_ptr<char[]> Data(new char[Size]);
    mpz_get_str(Data.get(), STRING_BASE, Work.get());

    // Send data now
    SendPrime(ClientSock.ClientSocket, Data.get(), Size);
}

bool NetworkController::ReportWork(__mpz_struct& WorkItem)
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    bool Success;
    int SentData = 0;
    size_t Size = mpz_sizeinbase(&WorkItem, STRING_BASE) + 2;
    NetworkHeader Header = { 0 };
    Header.Type = NetworkMessageType::ReportWork;
    Header.Size = htonl(Size);

    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        shutdown(Socket, SD_SEND);
        closesocket(Socket);
        return false;
    }

    // Using this instead of mpz_class::get_str because that
    // appears to have a memory leak.
    std::unique_ptr<char[]> Data(new char[Size]);
    mpz_get_str(Data.get(), STRING_BASE, &WorkItem);

    // Send data now
    Success = SendPrime(Socket, Data.get(), Size);

    shutdown(Socket, SD_SEND);
    closesocket(Socket);
    return Success;
}

void NetworkController::HandleReportWork(NetworkConnectionInfo& ClientSock, int Size)
{
    // Not sending any data on this socket, so shutdown send
    shutdown(ClientSock.ClientSocket, SD_SEND);

    if (Size < 0 || Size > (INT_MAX >> 1))
    {
        std::cerr << __FUNCTION__ << " failed size requirements" << std::endl;
        return;
    }

    std::unique_ptr<char[]> Data(ReceivePrime(ClientSock.ClientSocket, nullptr, Size));

    if (Data == nullptr)
    {
        ReportError(" failed to recv workitem");
        return;
    }

    if (!Settings.FileSettings.Path.empty())
    {
        // TODO
        // open a file handle of the path:
        // c:\path\passed\in\<ipaddress>\<power-of-2>.txt
        // write Data to file
    }
    else
    {
        unique_mpz Work(new __mpz_struct);
        mpz_init_set_str(Work.get(), Data.get(), STRING_BASE);

        //std::cout << mpz_get_ui(Work.get()) << std::endl;
        gmp_printf("%Zd\n", Work.get());
    }
}

bool NetworkController::BatchReportWork(std::vector<unique_mpz>& WorkItems)
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    NetworkHeader Header = { 0 };
    int Size;
    int LastSize = 0;
    int NetSize;
    std::unique_ptr<char[]> Data;

    Header.Type = NetworkMessageType::BatchReportWork;
    Header.Size = htonl(WorkItems.size());

    // Send header with count of WorkItems
    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        shutdown(Socket, SD_SEND);
        closesocket(Socket);
        return false;
    }

    for (int i = 0; i < WorkItems.size(); i++)
    {
        Size = mpz_sizeinbase(WorkItems[i].get(), STRING_BASE) + 2;
        if (Size > LastSize)
        {
            Data.reset(new char[Size]);
            LastSize = Size;
        }
        else
        {
            memset(Data.get(), 0, LastSize); // Assumption: zeroing out memory is faster than malloc()
        }
        mpz_get_str(Data.get(), STRING_BASE, WorkItems[i].get());

        // Convert Size to network-format.
        NetSize = htonl(Size);

        Result = send(Socket, (char*)&NetSize, sizeof(NetSize), 0);
        if (!IsSocketValid(Result))
        {
            ReportError(" send size: " + std::to_string(i));
            shutdown(Socket, SD_SEND);
            closesocket(Socket);
            return false;
        }

        if (!SendPrime(Socket, Data.get(), Size))
        {
            ReportError(" send prime: " + std::to_string(i));
            shutdown(Socket, SD_SEND);
            closesocket(Socket);
            return false;
        }
    }

    closesocket(Socket);
    return true;
}

bool NetworkController::BatchReportWork(std::vector<mpz_class>& WorkItems)
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    NetworkHeader Header = { 0 };
    int Size;
    int LastSize = 0;
    int NetSize;
    std::string Data;

    Header.Type = NetworkMessageType::BatchReportWork;
    Header.Size = htonl(WorkItems.size());

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

void NetworkController::HandleBatchReportWork(NetworkConnectionInfo& ClientSock, int Count)
{
    NETSOCK Result;
    int Size;
    int LastSize = 0; // Size of previous allocation
    std::unique_ptr<char[]> Data;

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

        // Optimization to prevent many memory allocations
        if (Size > LastSize)
        {
            Data.reset(ReceivePrime(ClientSock.ClientSocket, nullptr, Size));
            LastSize = Size;

            if (Data.get() == nullptr)
            {
                ReportError(" recv prime " + std::to_string(i));
                return;
            }
        }
        else
        {
            memset(Data.get(), 0, LastSize); // Assumption: zeroing out memory is faster than malloc()
            if (ReceivePrime(ClientSock.ClientSocket, Data.get(), Size) == nullptr)
            {
                ReportError(" recv prime " + std::to_string(i));
                return;
            }
        }


        // Consider a threadpool to actually handle the file IO, since
        // that may be slow. The server should have enough memory to hold
        // all the data until it is written out.
        if (!Settings.FileSettings.Path.empty())
        {
            // TODO
            // open a file handle of the path:
            // c:\path\passed\in\<ipaddress>\<power-of-2>.txt
            // write Data to file
        }
        else
        {
            unique_mpz Work(new __mpz_struct);
            mpz_init_set_str(Work.get(), Data.get(), STRING_BASE);

            //std::cout << mpz_get_ui(Work.get()) << std::endl;
            gmp_printf("%Zd\n", Work.get());
        }
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
    Clients.erase(ClientSock.IPv6);

    NetworkHeader Header = { 0 };
    Header.Type = NetworkMessageType::ShutdownClient;

    send(ClientSock.ClientSocket, (char*)&Header, sizeof(Header), 0);

    std::cout << "Unregistered client: ";
    if (ClientSock.IPv4.sin_family == AF_INET)
    {
        std::cout << std::hex << ntohl(ClientSock.IPv4.sin_addr.s_addr);
    }
    else
    {
        for (unsigned char* idx = std::begin(ClientSock.IPv6.sin6_addr.u.Byte);
            idx < std::end(ClientSock.IPv6.sin6_addr.u.Byte);
            idx++)
        {
            std::cout << std::hex << idx;
        }
    }
    std::cout << std::endl;
}

void NetworkController::ShutdownClients()
{
    NetworkHeader Header = { NetworkMessageType::ShutdownClient, 0 };

    // for each client
    for (AddressType client : Clients)
    {
        // add back the client's port
        if (client.IPv4.sin_family == AF_INET)
        {
            client.IPv4.sin_port = CLIENT_PORT;
        }
        else
        {
            client.IPv6.sin6_port = CLIENT_PORT;
        }

        // open a socket to the client
        NETSOCK Socket = GetSocketTo(client.IPv6);

        // send shutdown message
        send(Socket, (char*)&Header, sizeof(Header), 0);

        closesocket(Socket);
    }
}

void NetworkController::HandleShutdownClient(NetworkConnectionInfo& ServerSock)
{
    if ((ServerSock.IPv4.sin_family == AF_INET) && (Settings.NetworkSettings.IPv4.sin_addr.s_addr == ServerSock.IPv4.sin_addr.s_addr) ||
        (ServerSock.IPv6.sin6_family == AF_INET6) && (memcmp(&Settings.NetworkSettings.IPv6, &ServerSock.IPv6, sizeof(Settings.NetworkSettings.IPv6)) == 0))
    {
        Bot->Stop();
    }
}

void NetworkController::ListenLoop()
{
    while (true)
    {
        NetworkConnectionInfo ConnInfo = { 0 };
        //memset(ConnInfo.get(), 0, sizeof(NetworkConnectionInfo));
        
        socklen_t ConnInfoAddrSize = sizeof(ConnInfo.IPv6);

        ConnInfo.ClientSocket = accept(ListenSocket, (sockaddr*)&ConnInfo.IPv6, &ConnInfoAddrSize);

        // Let the thread handle whether the socket is valid or not.
        //std::async(&NetworkController::HandleRequest, this, std::move(ConnInfo));
        OutstandingConnections.EnqueueWorkItem(std::move(ConnInfo));
    }
}

void NetworkController::ClientBind()
{
    AddressType ClientAny;

    // match IP family and port number of server, because I'm lazy
    if (Settings.NetworkSettings.IPv4.sin_family == AF_INET)
    {
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
        //Settings.NetworkSettings.IPv4.sin_port = SERVER_PORT;

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
        //Settings.NetworkSettings.IPv6.sin6_port = SERVER_PORT;

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
        2 * std::thread::hardware_concurrency(),
        std::bind(&NetworkController::HandleRequest, this, std::placeholders::_1, std::placeholders::_2)),
    CompleteConnections(
        std::thread::hardware_concurrency(),
        std::bind(&NetworkController::CleanupRequest, this, std::placeholders::_1, std::placeholders::_2)),
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
        // Threadpool unused on client, so kill it
        OutstandingConnections.Stop();
        CompleteConnections.Stop();
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
        std::async(std::bind(&NetworkController::ListenLoop, this));
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
