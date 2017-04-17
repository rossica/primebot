#include "fileio.h"
#include "pal.h"
#include "prime.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <functional>

#define STRING_BASE (62)

std::string GetPrimeFileName(AllPrimebotSettings& Settings, int Iterations)
{
    return
        std::to_string(Settings.PrimeSettings.Bitsize)
        + "-"
        + std::to_string(Settings.PrimeSettings.RngSeed)
        + "-"
        + std::to_string(Iterations)
        + ".txt";
}

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

bool WritePrimeToSingleFile(std::string BasePath, std::string Name, std::string Prime)
{
    bool Result;


    Result = MakeDirectory(BasePath.c_str());
    if (!Result)
    {
        std::cerr << "Failed to make directory: " << BasePath << std::endl;
        return Result;
    }

    std::string FullFilePath = BasePath + '/' + Name;

    std::ofstream File(FullFilePath, std::ios::app);
    if (!File.is_open())
    {
        std::cerr << "Failed to open file: " << FullFilePath << std::endl;
        return false;
    }

    File << Prime << "\n";
    File.close();
    return !File.fail();
}

bool WritePrimesToSingleFile(std::string BasePath, std::string Name, mpz_list& Primes)
{
    bool Result;

    Result = MakeDirectory(BasePath.c_str());
    if (!Result)
    {
        std::cerr << "Failed to make directory: " << BasePath << std::endl;
        return Result;
    }

    std::string FullFilePath = BasePath + '/' + Name;

    std::ofstream File(FullFilePath, std::ios::out | std::ios::app);
    if (!File.is_open())
    {
        std::cerr << "Failed to open file: " << FullFilePath << std::endl;
        return false;
    }

    for (mpz_class& p : Primes)
    {
        File << p.get_str(STRING_BASE) << "\n";
    }
    File.close();

    return !File.fail();
}

bool WritePrimesToSingleFile(std::string BasePath, std::string Name, std::vector<std::string>& Primes)
{
    bool Result;

    Result = MakeDirectory(BasePath.c_str());
    if (!Result)
    {
        std::cerr << "Failed to make directory: " << BasePath << std::endl;
        return Result;
    }

    std::string FullFilePath = BasePath + '/' + Name;

    std::ofstream File(FullFilePath, std::ios::out | std::ios::app);
    if (!File.is_open())
    {
        std::cerr << "Failed to open file: " << FullFilePath << std::endl;
        return false;
    }

    for (std::string& p : Primes)
    {
        File << p << "\n";
    }
    File.close();

    return !File.fail();
}
