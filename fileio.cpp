#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#if defined(_WIN32) || defined(_WIN64)
#pragma warning( push )
#pragma warning( disable: 4146 )
#pragma warning( disable: 4800 )
#endif
#include "gmpxx.h"
#if defined(_WIN32) || defined(_WIN64)
#pragma warning( pop )
#endif
#include "fileio.h"
#include "pal.h"
#include "prime.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <functional>

#define STRING_BASE (62)

std::string GetBasePrimeFileName(AllPrimebotSettings& Settings, int Iterations)
{
    return
        std::to_string(Settings.PrimeSettings.Bitsize)
        + "-"
        + std::to_string(Settings.PrimeSettings.RngSeed)
        + "-"
        + std::to_string(Iterations);
}

std::string GetPrimeFileName(AllPrimebotSettings& Settings, int Iterations)
{
    return
        GetBasePrimeFileName(Settings, Iterations)
        + ".txt";
}

std::string GetPrimeFileNameBinary(AllPrimebotSettings& Settings, int Iterations)
{
    return
        GetBasePrimeFileName(Settings, Iterations)
        + ".bin";
}

std::string GetPrimeBasePath(AllPrimebotSettings& Settings, int Iterations)
{
    return
        Settings.FileSettings.Path
            + "/"
            + GetBasePrimeFileName(Settings, Iterations);
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

bool WritePrimeToSingleFileBinary(std::string BasePath, std::string Name, std::string Prime)
{
    std::FILE* PrimeFile = nullptr;
    bool Result;
    bool WriteFirst = false;
    mpz_class First;

    Result = MakeDirectory(BasePath.c_str());
    if (!Result)
    {
        std::cerr << "Failed to make directory: " << BasePath << std::endl;
        return Result;
    }

    std::string FullFilePath = BasePath + '/' + Name;

    PrimeFile = std::fopen(FullFilePath.c_str(), "a+b");
    if (PrimeFile == nullptr)
    {
        std::cerr << "Failed to open file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
        return false;
    }

    // Determine if at the beginning of the file by seeking to the end.
    // This only affects read operations since the file was opened for append.
    if (std::fseek(PrimeFile, 0, SEEK_END))
    {
        std::cerr << "Failed to seek file, " << FullFilePath << " " << std::strerror(errno) << std::endl;
        std::fclose(PrimeFile);
        return false;
    }

    if (std::ftell(PrimeFile) == 0)
    {
        WriteFirst = true;
        std::rewind(PrimeFile);
    }
    else
    {
        // Read the first prime from the file.
        std::rewind(PrimeFile);
        if (!mpz_inp_raw(First.get_mpz_t(), PrimeFile))
        {
            std::cerr << "Failed reading prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
            std::fclose(PrimeFile);
            return false;
        }
        std::rewind(PrimeFile);
    }

    mpz_class Current(Prime, STRING_BASE);

    if (!WriteFirst)
    {
        Current -= First;
    }

    if (mpz_out_raw(PrimeFile, Current.get_mpz_t()) == 0)
    {
        std::cerr << "Failed writing first prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
        std::fclose(PrimeFile);
        return false;
    }

    std::fclose(PrimeFile);

    return true;
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

mpz_list ReadPrimesFromFile(std::string FullFilePath)
{
    mpz_list Primes;
    mpz_class Current;
    std::ifstream PrimeFile(FullFilePath, std::ios::in);

    std::string CurrentStr;

    if (!std::getline(PrimeFile, CurrentStr))
    {
        std::cerr << "Failed reading first prime from file, " << FullFilePath << std::endl;
        PrimeFile.close();
        return Primes;
    }

    mpz_class First(CurrentStr, STRING_BASE);
    Primes.push_back(First);

    while (std::getline(PrimeFile, CurrentStr))
    {
        Current = mpz_class(CurrentStr, STRING_BASE);
        Current += First;

        Primes.push_back(Current);
    }

    if (PrimeFile.fail())
    {
        std::cerr << "Failed to read from file, " << FullFilePath << std::endl;
    }

    PrimeFile.close();
    return Primes;
}

bool WritePrimesToSingleFileBinary(std::string BasePath, std::string Name, mpz_list& Primes)
{
    std::FILE* PrimeFile = nullptr;
    bool Result;
    bool WriteFirst = false;
    mpz_class First;

    Result = MakeDirectory(BasePath.c_str());
    if (!Result)
    {
        std::cerr << "Failed to make directory: " << BasePath << std::endl;
        return Result;
    }

    std::string FullFilePath = BasePath + '/' + Name;

    PrimeFile = std::fopen(FullFilePath.c_str(), "a+b");
    if (PrimeFile == nullptr)
    {
        std::cerr << "Failed to open file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
        return false;
    }

    // Determine if at the beginning of the file by seeking to the end.
    // This only affects read operations since the file was opened for append.
    if (std::fseek(PrimeFile, 0, SEEK_END))
    {
        std::cerr << "Failed to seek file, " << FullFilePath << " " << std::strerror(errno) << std::endl;
        std::fclose(PrimeFile);
        return false;
    }

    if (std::ftell(PrimeFile) == 0)
    {
        WriteFirst = true;
        std::rewind(PrimeFile);
    }
    else
    {
        // Read the first prime from the file.
        std::rewind(PrimeFile);
        if (!mpz_inp_raw(First.get_mpz_t(), PrimeFile))
        {
            std::cerr << "Failed reading prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
            std::fclose(PrimeFile);
            return false;
        }
        std::rewind(PrimeFile);
    }



    for (mpz_class& p : Primes)
    {
        if (WriteFirst)
        {
            // The file is new! Write the first prime
            if (mpz_out_raw(PrimeFile, p.get_mpz_t()) == 0)
            {
                std::cerr << "Failed writing first prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                std::fclose(PrimeFile);
                return false;
            }

            First = p;

            WriteFirst = false;
        }
        else
        {
            mpz_class Diff(p);
            Diff -= First;
            if (mpz_out_raw(PrimeFile, Diff.get_mpz_t()) == 0)
            {
                if (std::ferror(PrimeFile))
                {
                    std::cerr << "Failed writing prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                    std::fclose(PrimeFile);
                    return false;
                }
            }
        }

    }

    std::fclose(PrimeFile);

    return true;
}

bool WritePrimesToSingleFileBinary(std::string BasePath, std::string Name, std::vector<std::string>& Primes)
{
    std::FILE* PrimeFile = nullptr;
    bool Result;
    bool WriteFirst = false;
    mpz_class First;

    Result = MakeDirectory(BasePath.c_str());
    if (!Result)
    {
        std::cerr << "Failed to make directory: " << BasePath << std::endl;
        return Result;
    }

    std::string FullFilePath = BasePath + '/' + Name;

    PrimeFile = std::fopen(FullFilePath.c_str(), "a+b");
    if (PrimeFile == nullptr)
    {
        std::cerr << "Failed to open file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
        return false;
    }

    // Determine if at the beginning of the file by seeking to the end.
    // This only affects read operations since the file was opened for append.
    if (std::fseek(PrimeFile, 0, SEEK_END))
    {
        std::cerr << "Failed to seek file, " << FullFilePath << " " << std::strerror(errno) << std::endl;
        std::fclose(PrimeFile);
        return false;
    }

    if (std::ftell(PrimeFile) == 0)
    {
        WriteFirst = true;
        std::rewind(PrimeFile);
    }
    else
    {
        // Read the first prime from the file.
        std::rewind(PrimeFile);
        if (!mpz_inp_raw(First.get_mpz_t(), PrimeFile))
        {
            std::cerr << "Failed reading prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
            std::fclose(PrimeFile);
            return false;
        }
        std::rewind(PrimeFile);
    }

    for (std::string & pstr : Primes)
    {
        // This is inefficient, but there's no way around it.
        mpz_class Current(pstr, STRING_BASE);

        if (WriteFirst)
        {
            // The file is new! Write the first prime
            if (mpz_out_raw(PrimeFile, Current.get_mpz_t()) == 0)
            {
                std::cerr << "Failed writing first prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                std::fclose(PrimeFile);
                return false;
            }

            First = Current;

            WriteFirst = false;
        }
        else
        {
            Current -= First;
            if (mpz_out_raw(PrimeFile, Current.get_mpz_t()) == 0)
            {
                if (std::ferror(PrimeFile))
                {
                    std::cerr << "Failed writing prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                    std::fclose(PrimeFile);
                    return false;
                }
            }
        }
    }

    std::fclose(PrimeFile);

    return true;
}

mpz_list ReadPrimesFromFileBinary(std::string FullFilePath)
{
    mpz_list Primes;
    mpz_class First;
    mpz_class Current;

    std::FILE* PrimeFile = std::fopen(FullFilePath.c_str(), "rb");
    if (PrimeFile == nullptr)
    {
        std::cerr << "Failed to open file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
        return {};
    }

    // Read first prime for offset calculations
    if (!mpz_inp_raw(First.get_mpz_t(), PrimeFile))
    {
        std::cerr << "Failed reading first prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
        std::fclose(PrimeFile);
        return Primes;
    }

    Primes.push_back(First);

    while (!std::feof(PrimeFile) && !std::ferror(PrimeFile))
    {
        if (!mpz_inp_raw(Current.get_mpz_t(), PrimeFile))
        {
            if (!std::feof(PrimeFile))
            {
                std::cerr << "Failed reading prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                std::fclose(PrimeFile);
                return Primes;
            }
            else
            {
                break;
            }
        }

        Current += First;
        Primes.push_back(Current);
    }

    std::fclose(PrimeFile);

    return Primes;
}
