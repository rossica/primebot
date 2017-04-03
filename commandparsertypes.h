#pragma once


#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2ipdef.h>
#include <string>
#elif defined __linux__
#include <string.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <netdb.h>
#endif

#define SERVER_PORT (htons(60000))

struct NetworkControllerSettings
{
    union
    {
        sockaddr_in IPv4;
        sockaddr_in6 IPv6;
    };

    // If true: this is server, listen on address.
    // If false: check Client boolean
    bool Server;
    // If true: this is client, connect to address.
    // If false: this is standalone, don't use networking.
    bool Client;

    NetworkControllerSettings() :
        IPv6({ 0 }),
        Server(false),
        Client(false)
    {}
};

struct PrimeSettings
{
    // Use Async prime finding code instead of threadpool
    bool UseAsync;
    // Threads to use for threadpool or Async
    unsigned int ThreadCount;
    // Number of bits in the numbers being searched
    unsigned int Bitsize;
    // Optional seed for the RNG to generate numbers to search
    unsigned int RngSeed;

    PrimeSettings();
};

struct PrimeFileSettings
{
    // Directory to store discovered prime numbers in
    // If running as a server, subfolders will be by IP address
    std::string Path;
};

struct AllPrimebotSettings
{
    NetworkControllerSettings NetworkSettings;
    PrimeSettings PrimeSettings;
    PrimeFileSettings FileSettings;
    // Whether to run the program or not.
    // On failure, this should be false.
    bool Run;

    AllPrimebotSettings() :
        NetworkSettings(),
        PrimeSettings(),
        FileSettings(),
        Run(true)
    {}
};
