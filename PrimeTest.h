#pragma once
#include <climits>
#include <bitset>

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


int mod(int a, int b);

int modTimes(int a, int b, int modulus);

const int numberIntSize = sizeof(int)*CHAR_BIT;
int firstBitFromLeft(std::bitset<numberIntSize> binary);

int firstBitFromRight(std::bitset<numberIntSize> binary);

bool MillerRabin(int power);

bool isLikelyPrime(int workitem);

bool isLikelyPrime(mpz_class workitem);
