#pragma once

#include <string>
#include <vector>

#pragma warning( push )
#pragma warning( disable: 4146 )
#pragma warning( disable: 4800 )
#include "gmp.h"
#include "gmpxx.h"
#pragma warning( pop )

std::vector<mpz_class> findPrimes(int threadTotal, mpz_class start, mpz_class finish);
