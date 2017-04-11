#include "fileio.h"
#include "pal.h"
#include "prime.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <functional>

std::string GetPrimeBasePath(AllPrimebotSettings& Settings, int Iterations)
{
    return
        Settings.FileSettings.Path
            + "/"
            + std::to_string(Settings.PrimeSettings.Bitsize)
            + "-"
            + std::to_string(Settings.PrimeSettings.RngSeed)
            + "-"
            + std::to_string(Iterations);
}

bool WritePrimeToFile(std::string BasePath, std::string Prime)
{
    bool Result;

    // Hash prime
    std::hash<std::string> HashAlg;
    auto Hash = HashAlg(Prime);
    unsigned char * HashPtr = (unsigned char*) &Hash;

    // Create folder structure based on first two bytes of hash
    std::stringstream PathStream;
    PathStream
        << BasePath
        << '/' << std::hex << std::setw(2) << std::setfill('0') << (int) HashPtr[sizeof(Hash)-1]
        << '/' << std::hex << std::setw(2) << std::setfill('0') << (int) HashPtr[sizeof(Hash)-2];

    Result = MakeDirectory(PathStream.str().c_str());
    if (!Result)
    {
        return Result;
    }

    PathStream << '/' << std::hex << std::setw(sizeof(Hash)*2) << std::setfill('0') << Hash << ".txt";

    std::ofstream File(PathStream.str(), std::ios::app);
    if (!File.is_open())
    {
        std::cout << "Failed to open file" << std::endl;
        return false;
    }

    File << Prime << "\n";
    File.close();
    return !File.fail();
}