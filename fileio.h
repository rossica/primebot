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

std::string GetPrimeBasePath(AllPrimebotSettings& Settings, int Iterations);

bool WritePrimeToFile(std::string BasePath, std::string Prime);

