#pragma once
#include <string>
#include "commandparsertypes.h"
#include "gmpwrapper.h"

class PrimeFileIo
{
private:
    mpz_class First;
    const AllPrimebotSettings& Settings;
    std::string FileName;

    bool WritePrimeToSingleFile(std::string BasePath, std::string Name, std::string Prime);
    bool WritePrimeToSingleFileBinary(std::string BasePath, std::string Name, std::string Prime);

    bool WritePrimesToSingleFile(std::string BasePath, std::string Name, mpz_list& Primes);
    bool WritePrimesToSingleFile(std::string BasePath, std::string Name, std::vector<std::string>& Primes);

    mpz_list ReadPrimesFromFile(std::string FullFilePath);

    bool WritePrimesToSingleFileBinary(std::string BasePath, std::string Name, mpz_list& Primes);
    bool WritePrimesToSingleFileBinary(std::string BasePath, std::string Name, std::vector<std::string>& Primes);

    mpz_list ReadPrimesFromFileBinary(std::string FullFilePath);

public:
    PrimeFileIo(const AllPrimebotSettings& Settings);

    mpz_list ReadPrimes();

    bool WritePrime(std::string Prime);

    bool WritePrimes(mpz_list& Primes);
    bool WritePrimes(std::vector<std::string>& Primes);

    void PrintPrimes();
};
