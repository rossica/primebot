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

SmartFile make_smartfile(const char* FilePath, const char* Mode)
{
    return SmartFile(std::fopen(FilePath, Mode), std::fclose);
}

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

bool PrimeFileIo::CreatePrimeFile(const std::string& BasePath, const std::string& Name)
{
    // If File == nullptr, create the file and path because it doesn't exist yet.
    // If failure, then try again next time. This should save 8% CPU during printing.
    if (File == nullptr)
    {
        bool Result = MakeDirectory(BasePath.c_str());
        if (!Result)
        {
            std::cerr << "Failed to make directory: " << BasePath << std::endl;
            return Result;
        }

        FullFilePath = BasePath + '/' + Name;

        File = make_smartfile(FullFilePath.c_str(), "a+b");
        if (File == nullptr)
        {
            std::cerr << "Failed to open file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
            return false;
        }
        return true;
    }
    else
    {
        return true;
    }
}

bool PrimeFileIo::GetFirstPrimeFromFile()
{
    if (First.get_ui() == 0)
    {
        // Determine if at the beginning of the file by seeking to the end.
        // This only affects read operations since the file was opened for append.
        if (std::fseek(File.get(), 0, SEEK_END))
        {
            std::cerr << "Failed to seek file, " << FullFilePath << " " << std::strerror(errno) << std::endl;
            return false;
        }

        if (std::ftell(File.get()) == 0)
        {
            // Empty file, rewind.
            std::rewind(File.get());
        }
        else
        {
            // Read the first prime from the file.
            std::rewind(File.get());
            if (!mpz_inp_raw(First.get_mpz_t(), File.get()))
            {
                std::cerr << "Failed reading prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                return false;
            }
            std::rewind(File.get());
        }

        return true;
    }
    else
    {
        return true;
    }
}

bool PrimeFileIo::WritePrimeToTextFile(const std::string& BasePath, const std::string& Name, const std::string& Prime)
{
    bool Result;


    Result = MakeDirectory(BasePath.c_str());
    if (!Result)
    {
        std::cerr << "Failed to make directory: " << BasePath << std::endl;
        return Result;
    }

    FullFilePath = BasePath + '/' + Name;

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

bool PrimeFileIo::WritePrimeToBinaryFile(const std::string& BasePath, const std::string& Name, const std::string& Prime)
{
    bool Result;

    Result = CreatePrimeFile(BasePath, Name);
    if (!Result)
    {
        std::cerr << __FUNCTION__ << " failed to create file" << std::endl;
        return Result;
    }

    // If the file is non-empty, then it will save some time
    Result = GetFirstPrimeFromFile();
    if (!Result)
    {
        std::cerr << __FUNCTION__ << " failed to read first prime from file." << std::endl;
        return Result;
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

    if (mpz_out_raw(File.get(), Current.get_mpz_t()) == 0)
    {
        std::cerr << "Failed writing first prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
        return false;
    }

    return true;
}

bool PrimeFileIo::WritePrimesToTextFile(const std::string& BasePath, const std::string& Name, const mpz_list& Primes)
{
    bool Result;

    Result = MakeDirectory(BasePath.c_str());
    if (!Result)
    {
        std::cerr << "Failed to make directory: " << BasePath << std::endl;
        return Result;
    }

    FullFilePath = BasePath + '/' + Name;

    std::ofstream File(FullFilePath, std::ios::out | std::ios::app);
    if (!File.is_open())
    {
        std::cerr << "Failed to open file: " << FullFilePath << std::endl;
        return false;
    }

    for (const mpz_class& p : Primes)
    {
        File << p.get_str(STRING_BASE) << "\n";
    }
    File.close();

    return !File.fail();
}

bool PrimeFileIo::WritePrimesToTextFile(const std::string& BasePath, const std::string& Name, const std::vector<std::unique_ptr<char[]>>& Primes)
{
    bool Result;

    Result = MakeDirectory(BasePath.c_str());
    if (!Result)
    {
        std::cerr << "Failed to make directory: " << BasePath << std::endl;
        return Result;
    }

    FullFilePath = BasePath + '/' + Name;

    std::ofstream File(FullFilePath, std::ios::out | std::ios::app);
    if (!File.is_open())
    {
        std::cerr << "Failed to open file: " << FullFilePath << std::endl;
        return false;
    }

    for (auto& p : Primes)
    {
        File << p.get() << "\n";
    }
    File.close();

    return !File.fail();
}

mpz_list PrimeFileIo::ReadPrimesFromTextFile(const std::string& FullFilePath)
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
        //Current += First; // TODO: uncomment when implement differencing to text path

        Primes.push_back(Current);
    }

    if (PrimeFile.fail())
    {
        std::cerr << "Failed to read from file, " << FullFilePath << std::endl;
    }

    PrimeFile.close();
    return Primes;
}

bool PrimeFileIo::WritePrimesToBinaryFile(const std::string& BasePath, const std::string& Name, const mpz_list& Primes)
{
    bool Result;

    Result = CreatePrimeFile(BasePath, Name);
    if (!Result)
    {
        std::cerr << __FUNCTION__ << " failed to create file" << std::endl;
        return Result;
    }

    // If the file is non-empty, then it will save some time
    Result = GetFirstPrimeFromFile();
    if (!Result)
    {
        std::cerr << __FUNCTION__ << " failed to read first prime from file." << std::endl;
        return Result;
    }

    auto pitr = Primes.begin();
    if (First == 0)
    {
        // The file is new! Write the first prime
        if (mpz_out_raw(File.get(), pitr->get_mpz_t()) == 0)
        {
            std::cerr << "Failed writing first prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
            return false;
        }

        First = *pitr;

        ++pitr;
    }

    for (; pitr != Primes.end(); ++pitr)
    {
        mpz_class Diff(*pitr);
        Diff -= First;
        if (mpz_out_raw(File.get(), Diff.get_mpz_t()) == 0)
        {
            if (std::ferror(File.get()))
            {
                std::cerr << "Failed writing prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                return false;
            }
        }
    }

    if (IoCount++ % FlushInterval == 0)
    {
        std::fflush(File.get());
    }

    return true;
}

bool PrimeFileIo::WritePrimesToBinaryFile(const std::string& BasePath, const std::string& Name, const std::vector<std::unique_ptr<char[]>>& Primes)
{
    bool Result;

    Result = CreatePrimeFile(BasePath, Name);
    if (!Result)
    {
        std::cerr << __FUNCTION__ << " failed to create file" << std::endl;
        return Result;
    }

    // If the file is non-empty, then it will save some time
    Result = GetFirstPrimeFromFile();
    if (!Result)
    {
        std::cerr << __FUNCTION__ << " failed to read first prime from file." << std::endl;
        return Result;
    }

    auto pitr = Primes.begin();
    if (First == 0)
    {
        mpz_class Temp((*pitr).get(), STRING_BASE);

        // The file is new! Write the first prime
        if (mpz_out_raw(File.get(), Temp.get_mpz_t()) == 0)
        {
            std::cerr << "Failed writing first prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
            return false;
        }

        First = Temp;

        ++pitr;
    }

    for (;pitr != Primes.end(); ++pitr)
    {
        mpz_class Current((*pitr).get(), STRING_BASE);

        Current -= First;
        if (mpz_out_raw(File.get(), Current.get_mpz_t()) == 0)
        {
            if (std::ferror(File.get()))
            {
                std::cerr << "Failed writing prime to file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
                return false;
            }
        }
    }

    if (IoCount++ % FlushInterval == 0)
    {
        std::fflush(File.get());
    }

    return true;
}

mpz_list PrimeFileIo::ReadPrimesFromBinaryFile(std::string FullFilePath)
{
    mpz_list Primes;
    mpz_class Current;

    SmartFile PrimeFile = make_smartfile(FullFilePath.c_str(), "rb");
    if (PrimeFile == nullptr)
    {
        std::cerr << "Failed to open file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
        return {};
    }

    // Read first prime for offset calculations
    if (!mpz_inp_raw(First.get_mpz_t(), PrimeFile.get()))
    {
        std::cerr << "Failed reading first prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
        return Primes;
    }

    Primes.push_back(First);

    while (!std::feof(PrimeFile.get()) && !std::ferror(PrimeFile.get()))
    {
        if (!mpz_inp_raw(Current.get_mpz_t(), PrimeFile.get()))
        {
            if (!std::feof(PrimeFile.get()))
            {
                std::cerr << "Failed reading prime from file, " << FullFilePath << ": " << std::strerror(errno) << std::endl;
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

    return Primes;
}

PrimeFileIo::PrimeFileIo(const AllPrimebotSettings & Settings) :
    First(0),
    Settings(Settings),
    File(nullptr, std::fclose)
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
        return ReadPrimesFromBinaryFile(Settings.FileSettings.Path);
    }
    else
    {
        return ReadPrimesFromTextFile(Settings.FileSettings.Path);
    }
}

bool PrimeFileIo::WritePrime(std::string Prime)
{
    if (Settings.FileSettings.Flags.Binary)
    {
        return WritePrimeToBinaryFile(Settings.FileSettings.Path, FileName, Prime);
    }
    else
    {
        return WritePrimeToTextFile(Settings.FileSettings.Path, FileName, Prime);
    }
}

bool PrimeFileIo::WritePrimes(const mpz_list & Primes)
{
    if (Settings.FileSettings.Flags.Binary)
    {
        return WritePrimesToBinaryFile(Settings.FileSettings.Path, FileName, Primes);
    }
    else
    {
        return WritePrimesToTextFile(Settings.FileSettings.Path, FileName, Primes);
    }
}

bool PrimeFileIo::WritePrimes(const std::vector<std::unique_ptr<char[]>>& Primes)
{
    if (Settings.FileSettings.Flags.Binary)
    {
        return WritePrimesToBinaryFile(Settings.FileSettings.Path, FileName, Primes);
    }
    else
    {
        return WritePrimesToTextFile(Settings.FileSettings.Path, FileName, Primes);
    }
}

void PrimeFileIo::PrintPrimes()
{

    // TODO: Print primes as they are read instead of making a list of them
    // then printing. This is necessary when the lists become very large.

    mpz_list Primes;
    if (Settings.FileSettings.Flags.Binary)
    {
        Primes = ReadPrimesFromBinaryFile(Settings.FileSettings.Path);
    }
    else
    {
        Primes = ReadPrimesFromTextFile(Settings.FileSettings.Path);
    }

    for (mpz_class & p : Primes)
    {
        // Compromise: gmp_printf on windows can't figure out the
        // stdio file handles. But this works fine.
        gmp_fprintf(stdout, "%Zd\n", p.get_mpz_t());
    }
}
