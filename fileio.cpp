#include "fileio.h"
#include "pal.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <functional>

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
        << '/' << std::hex << (int) HashPtr[sizeof(Hash)-1]
        << '/' << std::hex << (int) HashPtr[sizeof(Hash)-2];

    Result = MakeDirectory(PathStream.str().c_str());
    if (!Result)
    {
        return Result;
    }

    PathStream << '/' << std::hex << Hash << ".txt";

    std::ofstream File(PathStream.str());
    if (!File.is_open())
    {
        std::cout << "Failed to open file" << std::endl;
        return false;
    }

    File << Prime;
    File.close();
    return true;
}