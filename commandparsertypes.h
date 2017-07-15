#pragma once

#include <string>

#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX // windows.h provides MIN/MAX macros that conflict with min()/max() in gmpxx.h
#include <winsock2.h>
#include <ws2ipdef.h>
#elif defined __linux__
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
    // Optional starting number. If specified, RNG is not used.
    std::string StartValue;
    // Numeric Base of the given StartValue
    unsigned int StartValueBase;
    // Count of numbers to search for primes
    unsigned int BatchSize;
    // Total count of batches to find and send
    unsigned long long BatchCount;

    PrimebotSettings();
};

struct PrimeFileSettings
{
    // Directory to store discovered prime numbers in
    std::string Path;
    struct
    {
        // Whether to use binary or text mode to store primes.
        int Binary : 1;
        // Whether to just print out primes in file Path
        int Print : 1;
    } Flags;

    PrimeFileSettings() :
        Path()
    {
        Flags.Binary = true;
        Flags.Print = false;
    }
};

struct AllPrimebotSettings
{
    NetworkControllerSettings NetworkSettings;
    PrimebotSettings PrimeSettings;
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
