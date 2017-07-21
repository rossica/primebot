#pragma once

#include <string>
#include <vector>

#include "gmpwrapper.h"

std::vector<mpz_class> findPrimes(int threadTotal, mpz_class start, mpz_class finish);
