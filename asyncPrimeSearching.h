#pragma once

#include <string>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#pragma warning( push )
#pragma warning( disable: 4146 )
#pragma warning( disable: 4800 )
#endif
#include "gmp.h"
#include "gmpxx.h"
#if defined(_WIN32) || defined(_WIN64)
#pragma warning( pop )
#endif

std::vector<mpz_class> findPrimes(int threadTotal, mpz_class start, mpz_class finish);
