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
    bool ConfigureReverse(AllPrimebotSettings& Settings);
    bool ConfigureThreads(AllPrimebotSettings& Settings, std::string Threads);
    bool ConfigurePath(AllPrimebotSettings& Settings, std::string Path);
    bool ConfigureSeed(AllPrimebotSettings& Settings, std::string Seed);
    bool ConfigureBits(AllPrimebotSettings& Settings, std::string Bits);
    bool ConfigurePrint(AllPrimebotSettings& Settings);
    bool ConfigureText(AllPrimebotSettings& Settings);
    bool ConfigureBatchSize(AllPrimebotSettings& Settings, std::string Size);
    bool ConfigureBatches(AllPrimebotSettings& Settings, std::string Batches);
    bool ConfigureStartValue(AllPrimebotSettings& Settings, std::string Start);
    bool ConfigureStartValueBase(AllPrimebotSettings& Settings, std::string StartBase);
public:
    CommandParser(int argc, char** argv);
    AllPrimebotSettings ParseArguments();
};
