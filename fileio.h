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

    bool WritePrimeToTextFile(std::string BasePath, std::string Name, std::string Prime);
    bool WritePrimeToBinaryFile(std::string BasePath, std::string Name, std::string Prime);

    bool WritePrimesToTextFile(std::string BasePath, std::string Name, mpz_list& Primes);
    bool WritePrimesToTextFile(std::string BasePath, std::string Name, std::vector<std::unique_ptr<char[]>>& Primes);

    mpz_list ReadPrimesFromTextFile(std::string FullFilePath);

    bool WritePrimesToBinaryFile(std::string BasePath, std::string Name, mpz_list& Primes);
    bool WritePrimesToBinaryFile(std::string BasePath, std::string Name, std::vector<std::unique_ptr<char[]>>& Primes);

    mpz_list ReadPrimesFromBinaryFile(std::string FullFilePath);

public:
    PrimeFileIo(const AllPrimebotSettings& Settings);

    mpz_list ReadPrimes();

    bool WritePrime(std::string Prime);

    bool WritePrimes(mpz_list& Primes);
    bool WritePrimes(std::vector<std::unique_ptr<char[]>>& Primes);

    void PrintPrimes();
};
