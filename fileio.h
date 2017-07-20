#pragma once
#include <string>
#include "commandparsertypes.h"
#include "gmpwrapper.h"


typedef std::unique_ptr<std::FILE, decltype(&std::fclose)> SmartFile;

SmartFile make_smartfile(const char* FilePath, const char* Mode);

class PrimeFileIo
{
private:
    mpz_class First;
    const AllPrimebotSettings& Settings;
    std::string FileName;
    std::string FullFilePath;
    SmartFile File;
    unsigned int IoCount;
    const int FlushInterval = 10;

    bool CreatePrimeFile(const std::string& BasePath, const std::string& Name);
    bool GetFirstPrimeFromFile();

    bool WritePrimeToTextFile(const std::string& BasePath, const std::string& Name, const std::string& Prime);
    bool WritePrimeToBinaryFile(const std::string& BasePath, const std::string& Name, const std::string& Prime);

    bool WritePrimesToTextFile(const std::string& BasePath, const std::string& Name, const mpz_list& Primes);
    bool WritePrimesToTextFile(const std::string& BasePath, const std::string& Name, const std::vector<std::unique_ptr<char[]>>& Primes);

    mpz_list ReadPrimesFromTextFile(const std::string& FullFilePath);

    bool WritePrimesToBinaryFile(const std::string& BasePath, const std::string& Name, const mpz_list& Primes);
    bool WritePrimesToBinaryFile(const std::string& BasePath, const std::string& Name, const std::vector<std::unique_ptr<char[]>>& Primes);

    mpz_list ReadPrimesFromBinaryFile(std::string FullFilePath);

public:
    PrimeFileIo(const AllPrimebotSettings& Settings);

    mpz_list ReadPrimes();

    bool WritePrime(std::string Prime);

    bool WritePrimes(const mpz_list& Primes);
    bool WritePrimes(const std::vector<std::unique_ptr<char[]>>& Primes);

    void PrintPrimes();
};
