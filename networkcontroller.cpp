#include "networkcontroller.h"
#include <future>
#include "prime.h"
#include <iostream>
#include <string>


#if defined(_WIN32) || defined(_WIN64)
#define ReportError(Msg) std::cerr << WSAGetLastError() << " at " << Msg << std::endl;
#elif defined __linux__
#define ReportError(Msg) throw std::system_error(errno, std::system_category(), "");
#endif

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
        return;
    }

    //Header.Size = ntohl(Header.Size);

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
    closesocket(ClientSock.ClientSocket);
    //pool.EnqueueResult(std::move(ClientSock));
}

void NetworkController::CleanupRequest(NetworkConnectionInfo ClientSock)
{
    // When finished, close the client socket
    shutdown(ClientSock.ClientSocket, SD_BOTH);
    closesocket(ClientSock.ClientSocket);
}

void NetworkController::RegisterClient()
{
    NetworkHeader Header = { 0 };
    NETSOCK Socket = GetSocketToServer();

    Header.Type = NetworkMessageType::RegisterClient;
    if (!IsSocketValid(send(Socket, (char*)&Header, sizeof(Header), 0)))
    {
        ReportError(__FUNCTION__);
    }

    closesocket(Socket);
}

void NetworkController::HandleRegisterClient(NetworkConnectionInfo& ClientSock)
{
    // This should also copy the IPv4 data if it's only IPv4.
    Clients.insert(ClientSock.IPv6);
}

int NetworkController::RequestWork()
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    NetworkHeader Header = { 0 };
    Header.Type = NetworkMessageType::RequestWork;

    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(__FUNCTION__" send header");
    }

    Result = recv(Socket, (char*)&Header, sizeof(Header), MSG_WAITALL);
    if (!IsSocketValid(Result) || Result == 0)
    {
        ReportError(__FUNCTION__" recv header");
    }

    if (Header.Size < 1 || Header.Size >(INT_MAX >> 1))
    {
        throw std::invalid_argument("workitem size is wrong");
    }

    // yep, I'm allocating memory based on what a remote host tells me.
    // This is **VERY** INSECURE. Don't do it on networks that aren't secure.
    std::unique_ptr<char[]> Data(new char[Header.Size]);

    if (!IsSocketValid(recv(Socket, Data.get(), Header.Size, 0)))
    {
        ReportError(__FUNCTION__" recv data");
    }

    // TODO: import Data into an mpz_struct

    closesocket(Socket);
    return *(int*)Data.get();
}

void NetworkController::HandleRequestWork(NetworkConnectionInfo& ClientSock)
{
    NetworkHeader Header = { 0 };
    NETSOCK Result;
    int Data;

    Header.Type = NetworkMessageType::WorkItem;
    Header.Size = sizeof(int);
    
    //TODO Generate large random number
    Data = 3;

    // Tell client how large data is
    Result = send(ClientSock.ClientSocket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        return;
    }

    // send number back to requestor
    Result = send(ClientSock.ClientSocket, (char*)&Data, sizeof(Data), 0);
}

void NetworkController::ReportWork(int WorkItem)
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    NetworkHeader Header = { 0 };
    Header.Type = NetworkMessageType::ReportWork;
    Header.Size = sizeof(WorkItem);

    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(__FUNCTION__);
    }

    // Send data now
    send(Socket, (char*)&WorkItem, sizeof(WorkItem), 0);

    closesocket(Socket);
}

void NetworkController::HandleReportWork(NetworkConnectionInfo& ClientSock, int Size)
{
    int Result = 0;

    if (Size < 0 || Size > (INT_MAX >> 1))
    {
        return;
    }

    // yep, I'm allocating memory based on what a remote host tells me.
    // This is **VERY** INSECURE. Don't do it on networks that aren't secure.
    std::unique_ptr<char[]> Data(new char[Size]);

    Result = recv(ClientSock.ClientSocket, Data.get(), Size, 0);
    if (!IsSocketValid(Result))
    {
        ReportError(__FUNCTION__);
        return;
    }

    // TODO
    // open a file handle of the path:
    // c:\path\passed\in\<ipaddress>\<hashofdata>.bin
    // write Data to file

    std::cout << *(int*)Data.get() << std::endl;
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
        
        int ConnInfoAddrSize = sizeof(ConnInfo.IPv6);

        ConnInfo.ClientSocket = accept(ListenSocket, (sockaddr*)&ConnInfo.IPv6, &ConnInfoAddrSize);

        // Let the thread handle whether the socket is valid or not.
        std::async(&NetworkController::HandleRequest, this, ConnInfo);
    }
}

void NetworkController::ClientBind()
{
    union
    {
        sockaddr_in IPv4;
        sockaddr_in6 IPv6;
    } ClientAny = { 0 };

    // match IP family and port number of server, because I'm lazy
    if (Settings.IPv4.sin_family == AF_INET)
    {
        ClientAny.IPv4.sin_port = htons(60001);//Settings.IPv4.sin_port;
        if (!IsSocketValid(
                bind(
                    ListenSocket,
                    (sockaddr*)&ClientAny.IPv4,
                    sizeof(ClientAny.IPv4))))
        {
            ReportError(__FUNCTION__" bind IPv4");
        }
    }
    else
    {
        ClientAny.IPv6.sin6_port = htons(60001);//Settings.IPv6.sin6_port;

        if (!IsSocketValid(
                bind(
                    ListenSocket,
                    (sockaddr*)&ClientAny.IPv6,
                    sizeof(ClientAny.IPv6))))
        {
            ReportError(__FUNCTION__"bind IPv6");
        }
    }
}

void NetworkController::ServerBind()
{
    if (Settings.IPv4.sin_family == AF_INET)
    {
        Settings.IPv4.sin_port = htons(60000);

        if (!IsSocketValid(
                bind(
                    ListenSocket,
                    (sockaddr*)&Settings.IPv4,
                    sizeof(Settings.IPv4))))
        {
            ReportError(__FUNCTION__" bind IPv4");
        }
    }
    else
    {
        Settings.IPv6.sin6_port = htons(60000);

        if (!IsSocketValid(
                bind(
                    ListenSocket,
                    (sockaddr*)&Settings.IPv6,
                    sizeof(Settings.IPv6))))
        {
            ReportError(__FUNCTION__" bind IPv6");
        }
    }
}

NETSOCK NetworkController::GetSocketTo(sockaddr_in6& Client)
{
    NETSOCK Result;
    NETSOCK Socket = socket(Client.sin6_family, SOCK_STREAM, IPPROTO_TCP);
    if (!IsSocketValid(Socket))
    {
        ReportError(__FUNCTION__" socket creation");
    }

    if (Client.sin6_family == AF_INET)
    {
        Result =
            connect(
                Socket,
                (sockaddr*)&Client,
                sizeof(sockaddr_in));
        if (!IsSocketValid(Result))
        {
            ReportError(__FUNCTION__" connect ipv4");
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
            ReportError(__FUNCTION__" connect IPv6");
        }
    }

    return Socket;
}

NETSOCK NetworkController::GetSocketToServer()
{
    return GetSocketTo(Settings.IPv6);
}

NetworkController::NetworkController(NetworkControllerSettings Config) :
    ListenSocket(INVALID_SOCKET),
    Settings(Config),
    Bot(nullptr)
    /*tp(
        2 * std::thread::hardware_concurrency(),
        std::bind(&NetworkController::HandleRequest, this, std::placeholders::_1, std::placeholders::_2),
        std::bind(&NetworkController::CleanupRequest, this, std::placeholders::_1, std::placeholders::_2))*/
{
#if defined(_WIN32) || defined(_WIN64)
    WSAData WsData;
    if (WSAStartup(MAKEWORD(2, 2), &WsData) != NO_ERROR)
    {
        ReportError(__FUNCTION__);
    }
#endif

    if (!Settings.Server)
    {
        // Threadpool unused on client, so kill it
        //tp.Stop();
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

    //tp.Stop();
}

void NetworkController::Start()
{
    int Result = 0;

    // Clients and servers both listen on a socket for incoming connections
    // Clients just need to make sure the connection is from the server.

    ListenSocket = socket(Settings.IPv4.sin_family, SOCK_STREAM, IPPROTO_TCP);
    if (!IsSocketValid(ListenSocket))
    {
        ReportError(__FUNCTION__" socket creation");
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
        ReportError(__FUNCTION__" listen");
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

