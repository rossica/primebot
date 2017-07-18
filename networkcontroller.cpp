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
#include <cassert>


#if defined(_WIN32) || defined(_WIN64)
#define ReportError(Msg) std::cerr << WSAGetLastError() << " at " << __FUNCTION__ << Msg << std::endl;
#elif defined __linux__
#define ReportError(Msg) std::cerr << strerror(errno) << " at " << __FUNCTION__ << Msg << std::endl
#endif

template<class Alpha, class Type>
void CalculateMovingAverage(Type& Accumulator, const Type& Value)
{
    typedef std::ratio<1, 1> One;
    // Validate that 0 <= Alpha <= 1.
    static_assert(std::ratio_less_equal<Alpha, One>::value && std::ratio_greater_equal<Alpha, std::ratio<0, 1>>::value,
        "Alpha must be in the range [0,1]");

    typedef std::ratio_subtract<One, Alpha> alphaComplement;

    //Accumulator = std::chrono::duration_cast<std::chrono::steady_clock::duration>((a * Value) + ((1.0 - a) * Accumulator));
    Accumulator = ((Alpha::num * Value) / Alpha::den) + ((alphaComplement::num * Accumulator) / alphaComplement::den);
}

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

uint64_t NetworkController::ReceiveId(NETSOCK Socket)
{
    uint64_t Id;
    NETSOCK Result = recv(Socket, (char*)&Id, sizeof(Id), 0);
    if (!IsSocketValid(Result) || Result == 0)
    {
        ReportError(" failed to recv Id");
        return INVALID_ID;
    }

    Id = ntohll(Id);

    return Id;
}

bool NetworkController::SendId(NETSOCK Socket, uint64_t Id)
{
    Id = htonll(Id);

    NETSOCK Result = send(Socket, (char*)&Id, sizeof(Id), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send ID");
        return false;
    }

    return true;
}

uint32_t NetworkController::ReceiveOffset(NETSOCK Socket)
{
    uint32_t Offset;
    NETSOCK Result = recv(Socket, (char*)&Offset, sizeof(Offset), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" failed to recv Offset");
        return 0;
    }

    Offset = ntohl(Offset);

    return Offset;
}

bool NetworkController::SendOffset(NETSOCK Socket, uint32_t Offset)
{
    Offset = htonl(Offset);

    NETSOCK Result = send(Socket, (char*)&Offset, sizeof(Offset), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send Offset");
        return false;
    }

    return true;
}

NetworkPendingWorkitem NetworkController::CreateWorkitem(uint16_t WorkitemCount, std::shared_ptr<NetworkClientInfo> Client)
{
    auto Now = std::chrono::steady_clock::now();
    // TODO: growth function for batchsize relative to prime magnitude:
    //      (BatchSize * WorkitemCount) * bitsize-of-prime * ln(2)
    // However, this means that BatchSize reflects about how many primes will
    // be found; i.e. instead of specifying the size of the work, it specifies
    // the size of the result, which is unintuitive. Come back to this decision.
    // It also doesn't guarantee that WorkitemSize is even.
    auto WorkitemSize = Settings.PrimeSettings.BatchSize * WorkitemCount;

    // Take lock now.
    std::lock_guard<std::mutex> lock(PendingWorkitemsLock);

    // Determine if OutstandingWorkitemsList.front() has not returned recently enough
    // and use that instead of NextWorkitem. This requires invalidating the existing
    // workitem.
    // Specifically:
    // * Are there outstanding workitems?
    // * The workitem hasn't already been received
    // * Has it been more than twice the average workitem completion time for the assigned client?
    // * Does the client exist in the list of registered clients anymore?
    //
    // Rejected thoughts below:
    // * can't use outstandingworkitems vs. clients b/c 1 client may have many threads
    // * can't use workitems are received, b/c what if none are?
    // * can't use average completion time for all clients > 0 b/c what if none return?
    if (FirstPendingWorkitem != nullptr &&
        FirstPendingWorkitem->DataReceived == false &&
        ((Now - FirstPendingWorkitem->SendTime) > (2 * FirstPendingWorkitem->AssignedClient->AvgCompletionTime) ||
        Clients.find(FirstPendingWorkitem->AssignedClient->Key) == Clients.end()))
    {
        // Assign to current client.
        FirstPendingWorkitem->AssignedClient = Client;

        Client->TotalAssignedWorkitems++;
        FirstPendingWorkitem->SendTime = Now;
        DuplicateAssignments++;

        return FirstPendingWorkitem->CopyWork();
    }

    NetworkPendingWorkitem Workitem(NextWorkitem, WorkitemSize, Client);

    // Add Workitem to map of outstanding workitems.
    auto InsertedWorkitem = PendingWorkitems.emplace(Workitem.Id, std::move(Workitem));

    if (!InsertedWorkitem.second)
    {
        std::cout << "Failed to insert workitem! This is very bad!!" << std::endl;
    }

    // Set the First pointer if this is the first item inserted.
    if (FirstPendingWorkitem == nullptr)
    {
        FirstPendingWorkitem = &InsertedWorkitem.first->second;
    }

    // Set the Last pointer if this is the first item inserted
    if (LastPendingWorkitem == nullptr)
    {
        LastPendingWorkitem = &InsertedWorkitem.first->second;
    }
    else
    {
        // Increment the Last pointer to the newly-inserted item
        LastPendingWorkitem->Next = &InsertedWorkitem.first->second;
        LastPendingWorkitem = LastPendingWorkitem->Next;
    }

    // Increment workitem for next request.
    NextWorkitem += WorkitemSize;

    Client->TotalAssignedWorkitems++;
    NewAssignments++;

    return InsertedWorkitem.first->second.CopyWork();
}

bool NetworkController::CompleteWorkitem(uint64_t Id, std::shared_ptr<NetworkClientInfo> Client, std::vector<std::unique_ptr<char[]>> ReceivedData)
{
    auto Now = std::chrono::steady_clock::now();

    // take lock, Look up workitem in map, add receiveddata to workitem
    std::lock_guard<std::mutex> OuterLock(PendingWorkitemsLock);

    auto PendingWorkitem = PendingWorkitems.find(Id);

    // Didn't find the workitem in the map anymore.
    if (PendingWorkitem == PendingWorkitems.end())
    {
        MissingReceives++;
        return false;
    }

    if (PendingWorkitem->second.DataReceived)
    {
        // Already received data for this workitem; bail.
        DuplicateReceives++;
        return false;
    }

    PendingWorkitem->second.Data = std::move(ReceivedData);
    PendingWorkitem->second.DataReceived = true;
    PendingWorkitem->second.ReceivedTime = Now;

    // Record reporting client... (why?)
    PendingWorkitem->second.ReportingClient = Client;
    Client->LastReceivedTime = Now;
    Client->TotalCompletedWorkitems++;

    // Wake up IO thread to handle this, potentially
    {
        std::unique_lock<std::mutex> IoLock(IoWaitLock);
        IoWaitVariable.notify_all();
    }

    return true;
}

NetworkPendingWorkitem NetworkController::RemoveWorkitem()
{
    // Take lock
    PendingWorkitemsLock.lock();
    // Take ownership of current workitem
    NetworkPendingWorkitem CompletedWorkitem(std::move(*FirstPendingWorkitem));

    // Update Last workitem pointer first
    if (LastPendingWorkitem == FirstPendingWorkitem)
    {
        LastPendingWorkitem = nullptr;
    }

    // Update the First workitem pointer
    FirstPendingWorkitem = FirstPendingWorkitem->Next;

    // Remove from map
    PendingWorkitems.erase(CompletedWorkitem.Id);

    // Unlock
    PendingWorkitemsLock.unlock();

    // Update average on reporting client
    if (CompletedWorkitem.AssignedClient == CompletedWorkitem.ReportingClient)
    {
        // Only update if the same client reported it as it was last assigned to.
        // Don't want to give credit/blame where it's not due.
        CalculateMovingAverage<NetworkClientInfo::MeanWeight>(
            CompletedWorkitem.ReportingClient->AvgCompletionTime,
            CompletedWorkitem.ReceivedTime - CompletedWorkitem.SendTime);
    }

    // Update average for all clients
    CalculateMovingAverage<NetworkController::MeanWeight>(
        AvgClientCompletionTime,
        CompletedWorkitem.ReceivedTime - CompletedWorkitem.SendTime);

    return std::move(CompletedWorkitem);
}

int NetworkController::LivingClientsCount()
{
    auto Now = std::chrono::steady_clock::now();
    int count = 0;

    for (auto& c : Clients)
    {
        if (!c.second->Zombie &&
            (Now - c.second->LastReceivedTime) < (3 * c.second->AvgCompletionTime))
        {
            // Assume client is still living and include in count
            count++;
        }
    }

    return count;
}



void NetworkController::HandleRequest(NetworkConnectionInfo&& ClientSock)
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
        if (Client == Clients.end())
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

    // When finished, close the socket
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
    auto InsertedClient =
        Clients.emplace(
            ClientSock.addr.IPv6,
            std::make_shared<NetworkClientInfo>());

    if (InsertedClient.second)
    {
        // If insertion succeeded, copy the key to the client
        InsertedClient.first->second->Key = InsertedClient.first->first;
    }

    std::cout << "Registered client: " << ClientSock.addr.ToString() << std::endl;
}

ClientWorkitem NetworkController::RequestWork(uint16_t Count)
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    NetworkHeader Header = { 0 };
    Header.Type = NetworkMessageType::RequestWork;
    Header.Size = sizeof(Count);
    ClientWorkitem Workitem;

    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        closesocket(Socket);
        return ClientWorkitem();
    }

    // Convert to network byte-order.
    Count = htons(Count);

    // Send message requesting Count number of workitems
    Result = send(Socket, (char*)&Count, sizeof(Count), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send request");
        closesocket(Socket);
        return ClientWorkitem();
    }

    // done sending, close send
    shutdown(Socket, SD_SEND);

    // Receive the response header
    Result = recv(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result) || Result == 0)
    {
        ReportError(" recv header");
        closesocket(Socket);
        return ClientWorkitem();
    }

    Header.Size = ntohl(Header.Size);

    if (Header.Size < 1 || Header.Size > (INT_MAX >> 1))
    {
        ReportError(" header size invalid");
        closesocket(Socket);
        return ClientWorkitem();
    }

    Workitem.Id = ReceiveId(Socket);
    if (Workitem.Id == INVALID_ID)
    {
        ReportError(" invalid ID");
        closesocket(Socket);
        return ClientWorkitem();
    }

    Workitem.Offset = ReceiveOffset(Socket);
    if (Workitem.Offset == 0)
    {
        ReportError(" invalid offset");
        closesocket(Socket);
        return ClientWorkitem();
    }

    std::unique_ptr<char[]> Data(ReceivePrime(Socket, Header.Size));

    if (Data == nullptr)
    {
        ReportError(" empty start prime");
        closesocket(Socket);
        return ClientWorkitem();
    }

    // Initialize the Workitem with the string returned from the server.
    Workitem.Start = mpz_class(Data.get(), STRING_BASE);

    // done receiving, close recv
    shutdown(Socket, SD_RECEIVE);
    closesocket(Socket);

    if (mpz_even_p(Workitem.Start.get_mpz_t()))
    {
        std::cout << "Server gave us an even number!!" << std::endl;
    }

    return Workitem;
}

void NetworkController::HandleRequestWork(NetworkConnectionInfo& ClientSock, std::shared_ptr<NetworkClientInfo> ClientInfo)
{
    NetworkHeader Header = { 0 };
    size_t Size;
    NETSOCK Result;
    uint16_t Count;

    // Receive the requested amount of work
    Result = recv(ClientSock.ClientSocket, (char*)&Count, sizeof(Count), 0);
    if (!IsSocketValid(Result) || Result == 0)
    {
        ReportError(" recv count");
        return;
    }

    // Convert count back to host byte-order.
    Count = ntohs(Count);

    // Get prime number to send to client
    NetworkPendingWorkitem& Work = CreateWorkitem(Count, ClientInfo);

    Size = mpz_sizeinbase(Work.Start.get_mpz_t(), STRING_BASE) + 2;

    Header.Type = NetworkMessageType::WorkItem;
    Header.Size = htonl((unsigned int) Size);

    // Send header to client
    Result = send(ClientSock.ClientSocket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        return;
    }

    // Send ID to client
    if (!SendId(ClientSock.ClientSocket, Work.Id))
    {
        ReportError(" send id");
        return;
    }

    // Send offset to client
    if (!SendOffset(ClientSock.ClientSocket, Work.Range))
    {
        ReportError(" send offset");
        return;
    }

    // send number back to requestor
    std::string Data = Work.Start.get_str(STRING_BASE);

    // Send data now
    SendPrime(ClientSock.ClientSocket, Data.c_str(), Size);
}

bool NetworkController::ReportWork(uint64_t Id, std::vector<mpz_class>& WorkItems)
{
    NETSOCK Socket = GetSocketToServer();
    NETSOCK Result;
    NetworkHeader Header = { 0 };
    int Size;
    int NetSize;

    std::cout << "Reporting " << WorkItems.size() << " results to server" << std::endl;

    Header.Type = NetworkMessageType::ReportWork;
    Header.Size = htonl((unsigned int) WorkItems.size());

    // Send header with count of WorkItems
    Result = send(Socket, (char*)&Header, sizeof(Header), 0);
    if (!IsSocketValid(Result))
    {
        ReportError(" send header");
        closesocket(Socket);
        return false;
    }

    if (!SendId(Socket, Id))
    {
        ReportError(" send Id");
        closesocket(Socket);
        return false;
    }

    for (mpz_class& WorkItem : WorkItems)
    {
        Size = mpz_sizeinbase(WorkItem.get_mpz_t(), STRING_BASE) + 2;
        std::string Data(WorkItem.get_str(STRING_BASE)); // suspect this leaks memory

        // Convert Size to network-format.
        NetSize = htonl(Size);

        Result = send(Socket, (char*)&NetSize, sizeof(NetSize), 0);
        if (!IsSocketValid(Result))
        {
            ReportError(" send size");
            closesocket(Socket);
            return false;
        }

        if (!SendPrime(Socket, Data.c_str(), Size))
        {
            ReportError(" send prime");
            closesocket(Socket);
            return false;
        }
    }

    // TODO: receive confirmation message here
    shutdown(Socket, SD_BOTH);
    closesocket(Socket);
    return true;
}

void NetworkController::HandleReportWork(NetworkConnectionInfo& ClientSock, int Count, std::shared_ptr<NetworkClientInfo> ClientInfo)
{
    NETSOCK Result;
    int Size;
    std::vector<std::unique_ptr<char[]>> BatchData;

    BatchData.reserve(Count);

    // Only receiving data here
    shutdown(ClientSock.ClientSocket, SD_SEND);

    uint64_t Id = ReceiveId(ClientSock.ClientSocket);
    if (Id == INVALID_ID)
    {
        ReportError(" invalid ID recv'd");
        return;
    }

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

        std::unique_ptr<char[]> Data = ReceivePrime(ClientSock.ClientSocket, Size);

        if (Data.get() == nullptr)
        {
            ReportError(" recv prime " + std::to_string(i));
            return;
        }

        BatchData.emplace_back(Data.release());
    }

    if (BatchData.size() != Count)
    {
        ReportError(" count of received primes is less than expected!!");
        // Don't complete the workitem so it will be reassigned.
        return;
    }

    CompleteWorkitem(Id, ClientInfo, std::move(BatchData));

    // TODO: send confirmation message here
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
            client.second->Zombie = true;
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

void NetworkController::IoLoop()
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> IoLock(IoWaitLock);
            IoWaitVariable.wait(IoLock, [this]() -> bool { return FirstPendingWorkitem != nullptr; });
        }

        // Assumption: Since this thread is the only thread that modifies
        // FirstPendingWorkitem (see RemoveWorkitem below),
        // we don't need to take a lock.
        // If DataReceived is being concurrently modified, also don't need
        // to take the lock because the next iteration we will read it (correctly)
        if (!FirstPendingWorkitem->DataReceived)
        {
            continue;
        }

        NetworkPendingWorkitem CurrentWorkitem(std::move(RemoveWorkitem()));

        if (Settings.FileSettings.Path.empty())
        {
            for (auto & d : CurrentWorkitem.Data)
            {
                mpz_class Work(d.get(), STRING_BASE);

                //std::cout << Work << std::endl; // linker error on windows
                gmp_fprintf(stdout, "%Zd\n", Work.get_mpz_t());
            }
        }
        else
        {
            FileIo.WritePrimes(CurrentWorkitem.Data);
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
            HandleRequest(std::move(ConnInfo));
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

NETSOCK NetworkController::GetSocketTo(const sockaddr_in6& Client)
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

NetworkController::NetworkController(const AllPrimebotSettings& Config) :
    Settings(Config),
    FileIo(Settings),
    OutstandingConnections(
        std::thread::hardware_concurrency(),
        std::bind(&NetworkController::HandleRequest, this, std::placeholders::_1)),
    ListenSocket(INVALID_SOCKET),
    Bot(nullptr),
    FirstPendingWorkitem(nullptr),
    LastPendingWorkitem(nullptr),
    AvgClientCompletionTime(0),
    NewAssignments(0),
    DuplicateAssignments(0),
    DuplicateReceives(0),
    MissingReceives(0)
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
    }
}

NetworkController::~NetworkController()
{
    if (IsSocketValid(ListenSocket))
    {
        shutdown(ListenSocket, SD_BOTH);
        closesocket(ListenSocket);
    }
#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif

    if (Settings.NetworkSettings.Server)
    {
        OutstandingConnections.Stop();
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
        InitialValue = Primebot::GetInitialWorkitem(Settings);
        NextWorkitem = InitialValue;

        // Start IO thread
        std::thread(&NetworkController::IoLoop, this).detach();

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

    // Lock the list of workitems to check it
    PendingWorkitemsLock.lock();
    if (FirstPendingWorkitem != nullptr &&
        !FirstPendingWorkitem->DataReceived &&
        (FirstPendingWorkitem->AssignedClient->Zombie ||
        Clients.find(FirstPendingWorkitem->AssignedClient->Key) == Clients.end()))
    {
        // Never going to complete, bail.
        PendingWorkitemsLock.unlock();
        std::cout << "First pending workitem assigned to zombie/missing client. Bailing on cleanup"
            << std::endl;
    }
    else
    {
        // wait for list of clients to go to zero, indicating all have unregistered.
        auto RemainingIo = PendingWorkitems.size();
        PendingWorkitemsLock.unlock();
        auto RemainingClients = LivingClientsCount();

        while (RemainingClients > 0 || RemainingIo > 0)
        {
            {
                std::unique_lock<std::mutex> IoLock(IoWaitLock);
                IoWaitVariable.notify_all();
            }

            std::cout << "Waiting 1 second for remaining living clients: " << RemainingClients
                << ", remaining I/Os: " << RemainingIo << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(1));


            RemainingClients = LivingClientsCount();

            // Lock to check on progress
            PendingWorkitemsLock.lock();
            RemainingIo = PendingWorkitems.size();
            if (FirstPendingWorkitem != nullptr &&
                !FirstPendingWorkitem->DataReceived &&
                (FirstPendingWorkitem->AssignedClient->Zombie ||
                    Clients.find(FirstPendingWorkitem->AssignedClient->Key) == Clients.end()))
            {
                PendingWorkitemsLock.unlock();
                // Unlock and bail because no further progress will be made
                std::cout << "Current pending workitem assigned to zombie/missing client. Bailing on cleanup"
                    << std::endl;
                break;
            }
            // Unlock for next iteration
            PendingWorkitemsLock.unlock();
        }
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

std::random_device NetworkPendingWorkitem::Rd;
std::mt19937_64 NetworkPendingWorkitem::Rng = std::mt19937_64(
    std::seed_seq {
        NetworkPendingWorkitem::Rd(),
        NetworkPendingWorkitem::Rd(),
        NetworkPendingWorkitem::Rd(),
        NetworkPendingWorkitem::Rd(),
        NetworkPendingWorkitem::Rd(),
        NetworkPendingWorkitem::Rd(),
        NetworkPendingWorkitem::Rd(),
        NetworkPendingWorkitem::Rd()
});
std::uniform_int_distribution<uint64_t> NetworkPendingWorkitem::Distribution = std::uniform_int_distribution<uint64_t>();
// Obfuscate the output of the RNG by XORing with this process-lifetime key
uint64_t NetworkPendingWorkitem::Key = (((uint64_t)NetworkPendingWorkitem::Rd() << 32) | NetworkPendingWorkitem::Rd());

NetworkPendingWorkitem::NetworkPendingWorkitem(mpz_class Value, uint32_t Offset, std::shared_ptr<NetworkClientInfo> Cli) :
    Id(Distribution(Rng) ^ Key),
    AssignedClient(Cli),
    Start(Value),
    Range(Offset),
    Next(nullptr),
    SendTime(std::chrono::steady_clock::now()),
    DataReceived(false)
{}

NetworkPendingWorkitem NetworkPendingWorkitem::CopyWork()
{
    NetworkPendingWorkitem Temp;
    Temp.Id = this->Id;
    Temp.Range = this->Range;
    Temp.Start = this->Start;

    return Temp;
}
