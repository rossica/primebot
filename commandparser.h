#pragma once

#include "commandparsertypes.h"

class CommandParser
{
private:
    int ArgCount;
    char** Args;

    void PrintHelp();
    bool ConfigureSocketAddress(AllPrimebotSettings& Settings, int Flags, std::string Address);
    bool ConfigureServer(AllPrimebotSettings& Settings, std::string Address);
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
