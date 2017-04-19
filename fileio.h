#pragma once
#include <string>
#include "commandparsertypes.h"
#include "prime.h"

std::string GetPrimeFileName(AllPrimebotSettings& Settings, int Iterations);
std::string GetPrimeFileNameBinary(AllPrimebotSettings& Settings, int Iterations);

std::string GetPrimeBasePath(AllPrimebotSettings& Settings, int Iterations);

bool WritePrimeToSingleFile(std::string BasePath, std::string Name, std::string Prime);
bool WritePrimeToSingleFileBinary(std::string BasePath, std::string Name, std::string Prime);

bool WritePrimesToSingleFile(std::string BasePath, std::string Name, mpz_list& Primes);
bool WritePrimesToSingleFile(std::string BasePath, std::string Name, std::vector<std::string>& Primes);

mpz_list ReadPrimesFromFile(std::string FullFilePath);

bool WritePrimesToSingleFileBinary(std::string BasePath, std::string Name, mpz_list& Primes);
bool WritePrimesToSingleFileBinary(std::string BasePath, std::string Name, std::vector<std::string>& Primes);

mpz_list ReadPrimesFromFileBinary(std::string FullFilePath);
