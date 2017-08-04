#include "commandparser.h"
#include <thread>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#include <WS2tcpip.h>
#elif defined (__linux__)
#include <string.h>
// remove this if linux ever implements memcpy_s
#define memcpy_s(dest, destsz, src, srcsz) memcpy(dest, src, destsz)
#endif

#define CompareArg(Arg, Val) (std::string(Arg) == Val)
#define CheckIndexValid(idx, arg) if(idx >= ArgCount) { std::cout << "Invalid end of Argument: " << arg << std::endl; break; }

void CommandParser::PrintHelp()
{
    std::cout << "Primebot" << std::endl;
    std::cout << "primebot Usage:" << std::endl;
    std::cout << "primebot [[-c | -s] <addr>] [-a] [-t <cnt>] [-p <path>] [-i <seed>] [-b <cnt>]" << std::endl;
    std::cout << "         [-r] [--batchsize <size>] [--batches <cnt>] [--start <start>]" << std::endl;
    std::cout << "         [--startbase <base>]" << std::endl;
    std::cout << "primebot [-h | -?]" << std::endl;
    std::cout << "  -c / --client: Configure as a client, connection to server at <address>" << std::endl;
    std::cout << "  -s / --server: Configure as a server" << std::endl;
    std::cout << "  -a / --async: Use async routines instead of threadpool" << std::endl;
    std::cout << "  -t / --threads: Threadcount for threadpool and async" << std::endl;
    std::cout << "  -p / --path: Path to save discovered primes at. Otherwise print to console" << std::endl;
    std::cout << "  -i / --seed: Use <seed> for RNG to find starting number" << std::endl;
    std::cout << "  -b / --bits: Search numbers using at most <count> bits" << std::endl;
    std::cout << "  -r / --reverse: Search numbers in reverse, i.e. down towards zero" << std::endl;
    std::cout << "  --print: Print primes stored in a file given with -p/--path" << std::endl;
    std::cout << "  --text: Store primes written with -p/--path as text instead of binary" << std::endl;
    std::cout << "  --batchsize: Count of numbers each thread will test for primality" << std::endl;
    std::cout << "  --batches: Count of batches to search" << std::endl;
    std::cout << "  --start: Use <start> to start searching from. Overrides RNG." << std::endl;
    std::cout << "  --startbase: Numeric base to interpret Start as (valid range is 2-62)" << std::endl;
    std::cout << std::endl;
}

bool CommandParser::ConfigureSocketAddress(AllPrimebotSettings & Settings, int Flags, std::string Address)
{
#if defined(_WIN32) || defined(_WIN64)
    WSAData WsData;
    int Error = WSAStartup(MAKEWORD(2, 2), &WsData);
    if (Error != NO_ERROR)
    {
        std::cout << gai_strerror(Error) << " at " << __FUNCTION__ << std::endl;
    }
#endif
    addrinfo Hints = { 0 };
    addrinfo* Results;
    int Result = 0;
    Hints.ai_flags = Flags;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_protocol = IPPROTO_TCP;

    Result = getaddrinfo(
        Address.c_str(),
        nullptr,
        &Hints,
        &Results
    );

    if (Result != 0)
    {
        std::cout << gai_strerror(Result) << " on argument: " << Address << std::endl;
        freeaddrinfo(Results);

        return false;
    }

    if (Results->ai_family == AF_INET)
    {
        memcpy_s(
            &Settings.NetworkSettings.IPv4,
            sizeof(Settings.NetworkSettings.IPv4),
            Results->ai_addr,
            Results->ai_addrlen);

        Settings.NetworkSettings.IPv4.sin_port = SERVER_PORT;
    }
    else
    {
        memcpy_s(
            &Settings.NetworkSettings.IPv6,
            sizeof(Settings.NetworkSettings.IPv6),
            Results->ai_addr,
            Results->ai_addrlen);

        Settings.NetworkSettings.IPv6.sin6_port = SERVER_PORT;
    }
    freeaddrinfo(Results);
    return true;
}

bool CommandParser::ConfigureServer(AllPrimebotSettings& Settings, std::string Address)
{
    Settings.NetworkSettings.Server = true;
    return ConfigureSocketAddress(Settings, AI_V4MAPPED | AI_PASSIVE, Address);
}

bool CommandParser::ConfigureClient(AllPrimebotSettings& Settings, std::string Address)
{
    Settings.NetworkSettings.Client = true;
    return ConfigureSocketAddress(Settings, AI_V4MAPPED, Address);
}

bool CommandParser::ConfigureAsync(AllPrimebotSettings& Settings)
{
    Settings.PrimeSettings.UseAsync = true;
    return true;
}

bool CommandParser::ConfigureReverse(AllPrimebotSettings& Settings)
{
    Settings.PrimeSettings.Reverse = true;
    return true;
}

bool CommandParser::ConfigureThreads(AllPrimebotSettings& Settings, std::string Threads)
{
    try
    {
        Settings.PrimeSettings.ThreadCount = std::stoul(Threads);
    }
    catch (std::invalid_argument ia)
    {
        std::cout << ia.what() << std::endl;
        return false;
    }
    return true;
}

bool CommandParser::ConfigurePath(AllPrimebotSettings& Settings, std::string Path)
{
    Settings.FileSettings.Path = Path;
    return true;
}

bool CommandParser::ConfigureSeed(AllPrimebotSettings& Settings, std::string Seed)
{
    try
    {
        Settings.PrimeSettings.RngSeed = std::stoul(Seed);
    }
    catch (std::invalid_argument ia)
    {
        std::cout << ia.what() << std::endl;
        return false;
    }
    return true;
}

bool CommandParser::ConfigureBits(AllPrimebotSettings& Settings, std::string Bits)
{
    try
    {
        Settings.PrimeSettings.Bitsize = std::stoul(Bits);
    }
    catch (std::invalid_argument& ia)
    {
        std::cout << ia.what() << std::endl;
        return false;
    }
    return true;
}

bool CommandParser::ConfigurePrint(AllPrimebotSettings & Settings)
{
    Settings.FileSettings.Flags.Print = true;
    return true;
}

bool CommandParser::ConfigureText(AllPrimebotSettings & Settings)
{
    Settings.FileSettings.Flags.Binary = false;
    return true;
}

bool CommandParser::ConfigureBatchSize(AllPrimebotSettings & Settings, std::string Size)
{
    try
    {
        Settings.PrimeSettings.BatchSize = std::stoul(Size);
    }
    catch (std::invalid_argument& ia)
    {
        std::cout << ia.what() << std::endl;
        return false;
    }
    return true;
}

bool CommandParser::ConfigureBatches(AllPrimebotSettings & Settings, std::string Batches)
{
    try
    {
        Settings.PrimeSettings.BatchCount = std::stoull(Batches);
    }
    catch (std::invalid_argument& ia)
    {
        std::cout << ia.what() << std::endl;
        return false;
    }
    return true;
}

bool CommandParser::ConfigureStartValue(AllPrimebotSettings & Settings, std::string Start)
{
    Settings.PrimeSettings.StartValue = Start;
    return true;
}

bool CommandParser::ConfigureStartValueBase(AllPrimebotSettings & Settings, std::string StartBase)
{
    try
    {
        Settings.PrimeSettings.StartValueBase = std::stoull(StartBase);
        if (Settings.PrimeSettings.StartValueBase < 2 || Settings.PrimeSettings.StartValueBase > 62)
        {
            std::cout << "Base for start must be between 2 and 62! You entered: "
                << Settings.PrimeSettings.StartValueBase << std::endl;
            return false;
        }
    }
    catch (std::invalid_argument& ia)
    {
        std::cout << ia.what() << std::endl;
        return false;
    }
    return true;
}

CommandParser::CommandParser(int argc, char ** argv) :
    ArgCount(argc),
    Args(argv)
{
}

AllPrimebotSettings CommandParser::ParseArguments()
{
    AllPrimebotSettings Settings;
    bool Result = true;

    for (int i = 1; i < ArgCount && Result; i++)
    {
        if (i == 0)
        {
            std::cout << Args[0] << std::endl;
            continue;
        }

        if (CompareArg("--client", Args[i]) || CompareArg("-c", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            Result = ConfigureClient(Settings, Args[i]);
        }
        else if (CompareArg("--server", Args[i]) || CompareArg("-s", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            Result = ConfigureServer(Settings, Args[i]);
        }
        else if (CompareArg("--async", Args[i]) || CompareArg("-a", Args[i]))
        {
            Result = ConfigureAsync(Settings);
        }
        else if (CompareArg("--threads", Args[i]) || CompareArg("-t", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            Result = ConfigureThreads(Settings, Args[i]);
        }
        else if (CompareArg("--path", Args[i]) || CompareArg("-p", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            Result = ConfigurePath(Settings, Args[i]);
        }
        else if (CompareArg("--seed", Args[i]) || CompareArg("-i", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            Result = ConfigureSeed(Settings, Args[i]);
        }
        else if (CompareArg("--bits", Args[i]) || CompareArg("-b", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            Result = ConfigureBits(Settings, Args[i]);
        }
        else if (CompareArg("--reverse", Args[i]) || CompareArg("-r", Args[i]))
        {
            Result = ConfigureReverse(Settings);
        }
        else if (CompareArg("--print", Args[i]))
        {
            Result = ConfigurePrint(Settings);
        }
        else if (CompareArg("--text", Args[i]))
        {
            Result = ConfigureText(Settings);
        }
        else if (CompareArg("--batchsize", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            Result = ConfigureBatchSize(Settings, Args[i]);
        }
        else if (CompareArg("--batches", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            Result = ConfigureBatches(Settings, Args[i]);
        }
        else if (CompareArg("--start", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            Result = ConfigureStartValue(Settings, Args[i]);
        }
        else if (CompareArg("--startbase", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            Result = ConfigureStartValueBase(Settings, Args[i]);
        }
        else if (CompareArg("--help", Args[i]) || CompareArg("-h", Args[i]) ||
            CompareArg("-?", Args[i]))
        {
            PrintHelp();
            Settings.Run = false;
        }
        else if (CompareArg("--", Args[i]))
        {
            break;
        }
        else
        {
            std::cout << "Invalid argument: " << Args[i] << std::endl;
            Settings.Run = false;
            break; // out of loop
        }
    }

    if (!Result)
    {
        // Error occurred, throw
        std::cout << "Invalid argument on command line. See above messages for details." << std::endl;
        Settings.Run = false;
    }

    return Settings;
}

PrimebotSettings::PrimebotSettings() :
    UseAsync(false),
    Reverse(false),
    ThreadCount(std::thread::hardware_concurrency()),
    Bitsize(512),
    RngSeed(1234),
    StartValue(),
    StartValueBase(0),
    BatchSize(1000),
    BatchCount(-1)
{}
