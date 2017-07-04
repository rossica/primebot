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

std::string GetBasePrimeFileName(const AllPrimebotSettings& Settings)
{
    if (Settings.PrimeSettings.StartValue.empty())
    {
        return
            std::to_string(Settings.PrimeSettings.Bitsize)
            + "-"
            + std::to_string(Settings.PrimeSettings.RngSeed);
    }
    else
    {
        return
            Settings.PrimeSettings.StartValue
            + "-"
            + std::to_string(Settings.PrimeSettings.StartValueBase);
    }
}

std::string GetPrimeFileName(const AllPrimebotSettings& Settings)
{
    return
        GetBasePrimeFileName(Settings)
        + ".txt";
}

std::string GetPrimeFileNameBinary(const AllPrimebotSettings& Settings)
{
    return
        GetBasePrimeFileName(Settings)
        + ".bin";
}

bool PrimeFileIo::WritePrimeToSingleFile(std::string BasePath, std::string Name, std::string Prime)
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

bool PrimeFileIo::WritePrimeToSingleFileBinary(std::string BasePath, std::string Name, std::string Prime)
{
    std::FILE* PrimeFile = nullptr;
    bool Result;

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
        std::rewind(PrimeFile);
    }
    else
    {
        // Read the first prime from the file.
        if (First == 0)
        {
            std::rewind(PrimeFile);
            if (!mpz_inp_raw(First.get_mpz_t(), PrimeFile))
            {
                std::cerr << "Failed reading prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                std::fclose(PrimeFile);
                return false;
            }
        }
        // assumption: First is never != 0 here
        std::rewind(PrimeFile);
    }

    mpz_class Current(Prime, STRING_BASE);

    if (First != 0)
    {
        Current -= First;
    }
    else
    {
        First = Current;
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

bool PrimeFileIo::WritePrimesToSingleFile(std::string BasePath, std::string Name, mpz_list& Primes)
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

bool PrimeFileIo::WritePrimesToSingleFile(std::string BasePath, std::string Name, std::vector<std::string>& Primes)
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

mpz_list PrimeFileIo::ReadPrimesFromFile(std::string FullFilePath)
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

bool PrimeFileIo::WritePrimesToSingleFileBinary(std::string BasePath, std::string Name, mpz_list& Primes)
{
    std::FILE* PrimeFile = nullptr;
    bool Result;

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
        std::rewind(PrimeFile);
    }
    else
    {
        if (First == 0)
        {
            // Read the first prime from the file.
            std::rewind(PrimeFile);
            if (!mpz_inp_raw(First.get_mpz_t(), PrimeFile))
            {
                std::cerr << "Failed reading prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                std::fclose(PrimeFile);
                return false;
            }
        }
        // assumption: First is never != 0 here
        std::rewind(PrimeFile);
    }

    for (mpz_class& p : Primes)
    {
        if (First == 0)
        {
            // The file is new! Write the first prime
            if (mpz_out_raw(PrimeFile, p.get_mpz_t()) == 0)
            {
                std::cerr << "Failed writing first prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                std::fclose(PrimeFile);
                return false;
            }

            First = p;

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

bool PrimeFileIo::WritePrimesToSingleFileBinary(std::string BasePath, std::string Name, std::vector<std::string>& Primes)
{
    std::FILE* PrimeFile = nullptr;
    bool Result;

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
        std::rewind(PrimeFile);
    }
    else
    {
        if (First == 0)
        {
            // Read the first prime from the file.
            std::rewind(PrimeFile);
            if (!mpz_inp_raw(First.get_mpz_t(), PrimeFile))
            {
                std::cerr << "Failed reading prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                std::fclose(PrimeFile);
                return false;
            }
        }
        // assumption: First is never != 0 here
        std::rewind(PrimeFile);
    }

    for (std::string & pstr : Primes)
    {
        // This is inefficient, but there's no way around it.
        mpz_class Current(pstr, STRING_BASE);

        if (First == 0)
        {
            // The file is new! Write the first prime
            if (mpz_out_raw(PrimeFile, Current.get_mpz_t()) == 0)
            {
                std::cerr << "Failed writing first prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                std::fclose(PrimeFile);
                return false;
            }

            First = Current;

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

mpz_list PrimeFileIo::ReadPrimesFromFileBinary(std::string FullFilePath)
{
    mpz_list Primes;
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

PrimeFileIo::PrimeFileIo(const AllPrimebotSettings & Settings) :
    First(0),
    Settings(Settings)
{
    if (Settings.FileSettings.Flags.Binary)
    {
        FileName = ::GetPrimeFileNameBinary(Settings);
    }
    else
    {
        FileName = ::GetPrimeFileName(Settings);
    }
}

mpz_list PrimeFileIo::ReadPrimes()
{
    if (Settings.FileSettings.Flags.Binary)
    {
        return ReadPrimesFromFileBinary(Settings.FileSettings.Path);
    }
    else
    {
        return ReadPrimesFromFile(Settings.FileSettings.Path);
    }
}

bool PrimeFileIo::WritePrime(std::string Prime)
{
    if (Settings.FileSettings.Flags.Binary)
    {
        return WritePrimeToSingleFileBinary(Settings.FileSettings.Path, FileName, Prime);
    }
    else
    {
        return WritePrimeToSingleFile(Settings.FileSettings.Path, FileName, Prime);
    }
}

bool PrimeFileIo::WritePrimes(mpz_list & Primes)
{
    if (Settings.FileSettings.Flags.Binary)
    {
        return WritePrimesToSingleFileBinary(Settings.FileSettings.Path, FileName, Primes);
    }
    else
    {
        return WritePrimesToSingleFile(Settings.FileSettings.Path, FileName, Primes);
    }
}

bool PrimeFileIo::WritePrimes(std::vector<std::string>& Primes)
{
    if (Settings.FileSettings.Flags.Binary)
    {
        return WritePrimesToSingleFileBinary(Settings.FileSettings.Path, FileName, Primes);
    }
    else
    {
        return WritePrimesToSingleFile(Settings.FileSettings.Path, FileName, Primes);
    }
}

void PrimeFileIo::PrintPrimes()
{
    mpz_list Primes;
    if (Settings.FileSettings.Flags.Binary)
    {
        Primes = ReadPrimesFromFileBinary(Settings.FileSettings.Path);
    }
    else
    {
        Primes = ReadPrimesFromFile(Settings.FileSettings.Path);
    }

    for (mpz_class & p : Primes)
    {
        // Compromise: gmp_printf on windows can't figure out the
        // stdio file handles. But this works fine.
        gmp_fprintf(stdout, "%Zd\n", p.get_mpz_t());
    }
}
