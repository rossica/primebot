#include "networkcontroller.h"
#include <future>
#include <iostream>


#if defined(_WIN32) || defined(_WIN64)
#define ReportError(Msg) std::cerr << WSAGetLastError() << " at " << __FUNCTION__ << Msg << std::endl;
#elif defined __linux__
#define ReportError(Msg) std::cerr << strerror(errno) << " at " << __FUNCTION__ << Msg << std::endl
#endif

#define STRING_BASE (62)

void NetworkController::HandleRequest(decltype(tp)& pool, NetworkConnectionInfo ClientSock)
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
    case NetworkMessageType::UnregisterClient:
        if (Clients.find(ClientSock.IPv6) != Clients.end())
        {
            HandleUnregisterClient(ClientSock);
        }
        break;
    case NetworkMessageType::ShutdownClient:
        if (!Settings.Server)
        {
            HandleShutdownClient(ClientSock);
        }
        break;
    default:
        return;
    }
    
    //// When finished, close the client socket
    shutdown(ClientSock.ClientSocket, SD_BOTH);
    pool.EnqueueResult(std::move(ClientSock));
}

void NetworkController::CleanupRequest(decltype(tp)& pool, NetworkConnectionInfo ClientSock)
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
        return nullptr;
    }

    shutdown(Socket, SD_SEND);

    Result = recv(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result) || Result == 0)
    {
        ReportError(" recv header");
        return nullptr;
    }

    Header.Size = ntohl(Header.Size);

    if (Header.Size < 1 || Header.Size > (INT_MAX >> 1))
    {
        ReportError(" header size invalid");
        return nullptr;
    }

    // yep, I'm allocating memory based on what a remote host tells me.
    // This is **VERY** INSECURE. Don't do it on networks that aren't secure.
    std::unique_ptr<char[]> Data(new char[Header.Size]);

    do {
        Result = recv(Socket, Data.get() + ReceivedData, Header.Size - ReceivedData, 0);
        if (!IsSocketValid(Result) || Result == 0)
        {
            ReportError(" recv data");
        }
        ReceivedData += Result;

    } while (IsSocketValid(Result) && Result > 0 && ReceivedData < Header.Size);

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
    gmp_randstate_t Rand;
    mp_bitcnt_t Bits = 512;

    gmp_randinit_mt(Rand);
    // TODO: make this seed configurable
    gmp_randseed_ui(Rand, 1234);

    unique_mpz Work(new __mpz_struct);
    //mpz_init_set_ui(Work.get(), 1);
    mpz_init(Work.get());
    mpz_urandomb(Work.get(), Rand, Bits);

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
    do {
        Result = send(ClientSock.ClientSocket, Data.get() + SentData, Size - SentData, 0);

        if (!IsSocketValid(Result))
        {
            ReportError(" send work");
        }
        SentData += Result;

    } while (IsSocketValid(Result) && SentData < Size);
}

bool NetworkController::ReportWork(__mpz_struct& WorkItem)
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    int SentData = 0;
    size_t Size = mpz_sizeinbase(&WorkItem, STRING_BASE) + 2;
    NetworkHeader Header = { 0 };
    Header.Type = NetworkMessageType::ReportWork;
    Header.Size = htonl(Size);

    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        return false;
    }

    // Using this instead of mpz_class::get_str because that
    // appears to have a memory leak.
    std::unique_ptr<char[]> Data(new char[Size]);
    mpz_get_str(Data.get(), STRING_BASE, &WorkItem);

    // Send data now
    do {
        Result = send(Socket, Data.get() + SentData, Size - SentData, 0);

        if (!IsSocketValid(Result))
        {
            ReportError(" send work");
            return false;
        }
        SentData += Result;

    } while (IsSocketValid(Result) && SentData < Size);

    shutdown(Socket, SD_SEND);
    closesocket(Socket);
    return true;
}

void NetworkController::HandleReportWork(NetworkConnectionInfo& ClientSock, int Size)
{
    NETSOCK Result;
    int ReceivedData = 0;

    // Not sending any data on this socket, so shutdown send
    shutdown(ClientSock.ClientSocket, SD_SEND);

    if (Size < 0 || Size > (INT_MAX >> 1))
    {
        std::cerr << __FUNCTION__ << " failed size requirements" << std::endl;
        return;
    }

    // yep, I'm allocating memory based on what a remote host tells me.
    // This is **VERY** INSECURE. Don't do it on networks that aren't secure.
    // We have very weak "authentication": only clients that have registered
    // with this server are allowed to send data.
    std::unique_ptr<char[]> Data(new char[Size]);

    do {
        Result = recv(ClientSock.ClientSocket, Data.get() + ReceivedData, Size - ReceivedData, 0);
        if (!IsSocketValid(Result) || Result == 0)
        {
            ReportError(" failed recv");
            return;
        }
        ReceivedData += Result;

    } while (IsSocketValid(Result) && Result > 0 && ReceivedData < Size);

    // TODO
    // open a file handle of the path:
    // c:\path\passed\in\<ipaddress>\<power-of-2>.txt
    // write Data to file

    unique_mpz Work(new __mpz_struct);
    mpz_init_set_str(Work.get(), Data.get(), STRING_BASE);

    //std::cout << mpz_get_ui(Work.get()) << std::endl;
    gmp_printf("%Zd\n", Work.get());
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
}

void NetworkController::ShutdownClients()
{
    NetworkHeader Header = { NetworkMessageType::ShutdownClient, 0 };

    // for each client
    for (AddressType client : Clients)
    {
        // open a socket to the client
        NETSOCK Socket = GetSocketTo(client.IPv6);

        // send shutdown message
        send(Socket, (char*)&Header, sizeof(Header), 0);

        closesocket(Socket);
    }
}

void NetworkController::HandleShutdownClient(NetworkConnectionInfo& ServerSock)
{
    if (memcmp(&Settings.IPv6, &ServerSock.IPv6, sizeof(Settings.IPv6)) == 0)
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
        tp.EnqueueWorkItem(std::move(ConnInfo));
    }
}

void NetworkController::ClientBind()
{
    AddressType ClientAny;

    // match IP family and port number of server, because I'm lazy
    if (Settings.IPv4.sin_family == AF_INET)
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
    if (Settings.IPv4.sin_family == AF_INET)
    {
        Settings.IPv4.sin_port = SERVER_PORT;

        if (!IsSocketValid(
                bind(
                    ListenSocket,
                    (sockaddr*)&Settings.IPv4,
                    sizeof(Settings.IPv4))))
        {
            ReportError(" bind IPv4");
        }
    }
    else
    {
        Settings.IPv6.sin6_port = SERVER_PORT;

        if (!IsSocketValid(
                bind(
                    ListenSocket,
                    (sockaddr*)&Settings.IPv6,
                    sizeof(Settings.IPv6))))
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
    return GetSocketTo(Settings.IPv6);
}

NetworkController::NetworkController(NetworkControllerSettings Config) :
    Settings(Config),
    tp(
        2 * std::thread::hardware_concurrency(),
        std::bind(&NetworkController::HandleRequest, this, std::placeholders::_1, std::placeholders::_2),
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

    if (!Settings.Server)
    {
        // Threadpool unused on client, so kill it
        tp.Stop();
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

    if (Settings.Server)
    {
        tp.Stop();
    }
}

void NetworkController::Start()
{
    int Result = 0;

    // Clients and servers both listen on a socket for incoming connections
    // Clients just need to make sure the connection is from the server.

    ListenSocket = socket(Settings.IPv4.sin_family, SOCK_STREAM, IPPROTO_TCP);
    if (!IsSocketValid(ListenSocket))
    {
        ReportError(" socket creation");
    }

    if (Settings.Server)
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
    if (Settings.Server)
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

