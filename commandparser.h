#pragma once
#include "pal.h"
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

struct PrimebotSettings
{
    // Use Async prime finding code instead of threadpool
    bool UseAsync;
    // Threads to use for threadpool or Async
    unsigned int ThreadCount;
    // Number of bits in the numbers being searched
    unsigned int Bitsize;
    // Optional seed for the RNG to generate numbers to search
    unsigned int RngSeed;

    PrimebotSettings();
};

struct PrimeFileSettings
{
    // Directory to store discovered prime numbers in
    // If running as a server, subfolders will be by IP address
    std::string Path;
};

struct AllSettings
{
    NetworkControllerSettings NetworkSettings;
    PrimebotSettings PrimeSettings;
    PrimeFileSettings FileSettings;

    AllSettings() :
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
    bool ConfigureServer(AllSettings& Settings);
    bool ConfigureClient(AllSettings& Settings, std::string Address);
    bool ConfigureAsync(AllSettings& Settings);
    bool ConfigureThreads(AllSettings& Settings, std::string Threads);
    bool ConfigurePath(AllSettings& Settings, std::string Path);
    bool ConfigureSeed(AllSettings& Settings, std::string Seed);
    bool ConfigureBits(AllSettings& Settings, std::string Bits);
public:
    CommandParser(int argc, char** argv);
    AllSettings ParseArguments();
};
