#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2ipdef.h>
#elif defined __linux__
#include <sys/types.h>
#include <netinet/ip.h>
#endif

#include <string>

#define SERVER_PORT (htons(60000))
#define CLIENT_PORT (htons(60001))

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

    NetworkControllerSettings() :
        IPv6({ 0 }),
        Server(false)
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

    AllPrimebotSettings() :
        NetworkSettings(),
        PrimeSettings(),
        FileSettings()
    {}
};

class CommandParser
{
private:
    int ArgCount;
    char** Args;

    void PrintHelp();
    bool ConfigureServer(AllPrimebotSettings& Settings);
    bool ConfigureClient(AllPrimebotSettings& Settings, std::string Address);
    bool ConfigureAsync(AllPrimebotSettings& Settings);
    bool ConfigureThreads(AllPrimebotSettings& Settings, std::string Threads);
    bool ConfigurePath(AllPrimebotSettings& Settings, std::string Path);
    bool ConfigureSeed(AllPrimebotSettings& Settings, std::string Seed);
    bool ConfigureBits(AllPrimebotSettings& Settings, std::string Bits);
public:
    CommandParser(int argc, char** argv);
    AllPrimebotSettings ParseArguments();
};
