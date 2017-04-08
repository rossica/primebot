#pragma once
#pragma warning( push )
#pragma warning( disable: 4146 )
#pragma warning( disable: 4800 )
#include "gmpxx.h"
#pragma warning( pop )
#include <string>

bool WritePrimeToFile(std::string BasePath, std::string Prime);

