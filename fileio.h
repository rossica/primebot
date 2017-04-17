#pragma once
#if defined(_WIN32) || defined(_WIN64)
#pragma warning( push )
#pragma warning( disable: 4146 )
#pragma warning( disable: 4800 )
#endif
#include "gmpxx.h"
#if defined(_WIN32) || defined(_WIN64)
#pragma warning( pop )
#endif
#include <string>
#include "commandparsertypes.h"
#include "prime.h"

std::string GetPrimeFileName(AllPrimebotSettings& Settings, int Iterations);

std::string GetPrimeBasePath(AllPrimebotSettings& Settings, int Iterations);

bool WritePrimeToSingleFile(std::string BasePath, std::string Name, std::string Prime);

bool WritePrimesToSingleFile(std::string BasePath, std::string Name, mpz_list& Primes);
bool WritePrimesToSingleFile(std::string BasePath, std::string Name, std::vector<std::string>& Primes);

