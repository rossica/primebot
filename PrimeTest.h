#pragma once
#include<climits>
#include<bitset>

#pragma warning( push )
#pragma warning( disable: 4146 )
#include "gmp.h"
#include "gmpxx.h"
#pragma warning( pop )


int mod(int a, int b);

int modTimes(int a, int b, int modulus);

const int numberIntSize = sizeof(int)*CHAR_BIT;
int firstBitFromLeft(std::bitset<numberIntSize> binary);

int firstBitFromRight(std::bitset<numberIntSize> binary);

bool MillerRabin(int power);

bool isLikelyPrime(int workitem);

bool isLikelyPrime(mpz_class workitem);
