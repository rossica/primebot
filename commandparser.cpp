#include "commandparser.h"

#include <thread>
#include <iostream>

#define CompareArg(Arg, Val) (std::string(Arg) == Val)
#define CheckIndexValid(idx, arg) if(idx >= ArgCount) { std::cout << "Invalid end of Argument: " << arg << std::endl; break; }

void CommandParser::PrintHelp()
{
    std::cout << "primebot Usage:" << std::endl;
    std::cout << "primebot [-c <addr> | -s] [-a] [-t <count>] [-p <path>] [-i <seed>] [-b <count>]" << std::endl;
    std::cout << "primebot [-h | -?]" << std::endl;
    std::cout << "  -c/--client: Configure as a client, connection to server at <address>" << std::endl;
    std::cout << "  -s/--server: Configure as a server" << std::endl;
    std::cout << "  -a/--async: Use async routines instead of threadpool" << std::endl;
    std::cout << "  -t/--threads: Threadcount for threadpool and async" << std::endl;
    std::cout << "  -p/--path: Path to save discovered primes at. Otherwise print to console" << std::endl;
    std::cout << "  -i/--seed: Use <seed> for RNG to find starting number" << std::endl;
    std::cout << "  -b/--bits: Search numbers using at most <count> bits" << std::endl;
    std::cout << std::endl;
}

bool CommandParser::ConfigureServer(AllSettings& Settings)
{
    Settings.NetworkSettings.Server = true;
    return true;
}

bool CommandParser::ConfigureClient(AllSettings& Settings, std::string Address)
{
    addrinfo Hints = { 0 };
    addrinfo* Results;
    int Result = 0;
    Hints.ai_flags = AI_V4MAPPED;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_protocol = IPPROTO_TCP;

    //Result = getaddrinfo(
    //    Address.c_str(),
    //    nullptr,
    //    &Hints,
    //    &Results
    //);

    //if (Result != 0)
    //{
    //    std::cout << gai_strerror(Result) << " on argument: " << Address << std::endl;
    //    freeaddrinfo(Results);

    //    return false;
    //}

    //if (Results->ai_family == AF_INET)
    //{
    //    memcpy_s(
    //        &Settings.NetworkSettings.IPv4,
    //        sizeof(Settings.NetworkSettings.IPv4),
    //        Results->ai_addr,
    //        Results->ai_addrlen);

    //    Settings.NetworkSettings.IPv4.sin_port = SERVER_PORT;
    //}
    //else
    //{
    //    memcpy_s(&Settings.NetworkSettings.IPv6,
    //        sizeof(Settings.NetworkSettings.IPv6),
    //        Results->ai_addr,
    //        Results->ai_addrlen);

    //    Settings.NetworkSettings.IPv6.sin6_port = SERVER_PORT;
    //}

    //freeaddrinfo(Results);
    return true;
}

bool CommandParser::ConfigureAsync(AllSettings& Settings)
{
    Settings.PrimeSettings.UseAsync = true;
    return true;
}

bool CommandParser::ConfigureThreads(AllSettings& Settings, std::string Threads)
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

bool CommandParser::ConfigurePath(AllSettings& Settings, std::string Path)
{
    Settings.FileSettings.Path = Path;
    return true;
}

bool CommandParser::ConfigureSeed(AllSettings& Settings, std::string Seed)
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

bool CommandParser::ConfigureBits(AllSettings& Settings, std::string Bits)
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

CommandParser::CommandParser(int argc, char ** argv) :
    ArgCount(argc),
    Args(argv)
{
}

AllSettings CommandParser::ParseArguments()
{
    AllSettings Settings;

    for (int i = 0; i < ArgCount; i++)
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
            ConfigureClient(Settings, Args[i]);
        }
        else if (CompareArg("--server", Args[i]) || CompareArg("-s", Args[i]))
        {
            ConfigureServer(Settings);
        }
        else if (CompareArg("--async", Args[i]) || CompareArg("-a", Args[i]))
        {
            ConfigureAsync(Settings);
        }
        else if (CompareArg("--threads", Args[i]) || CompareArg("-t", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            ConfigureThreads(Settings, Args[i]);
        }
        else if (CompareArg("--path", Args[i]) || CompareArg("-p", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            ConfigurePath(Settings, Args[i]);
        }
        else if (CompareArg("--seed", Args[i]) || CompareArg("-i", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            ConfigureSeed(Settings, Args[i]);
        }
        else if (CompareArg("--bits", Args[i]) || CompareArg("-b", Args[i]))
        {
            CheckIndexValid(i + 1, Args[i]);
            i++;
            ConfigureBits(Settings, Args[i]);
        }
        else if (CompareArg("--help", Args[i]) || CompareArg("-h", Args[i]) ||
            CompareArg("-?", Args[i]))
        {
            PrintHelp();
        }
        else if (CompareArg("--", Args[i]))
        {
            break;
        }
        else
        {
            std::cout << "Invalid argument: " << Args[i] << std::endl;
            break; // out of loop
        }
    }

    return Settings;
}

PrimebotSettings::PrimebotSettings() :
    UseAsync(false),
    ThreadCount(std::thread::hardware_concurrency()),
    Bitsize(256),
    RngSeed(1234)
{}
