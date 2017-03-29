#pragma once
#include<climits>
#include<bitset>


extern int mod(int a, int b);

extern int modTimes(int a, int b, int modulus);

const int numberIntSize = sizeof(int)*CHAR_BIT;
extern int firstBitFromLeft(std::bitset<numberIntSize> binary);

extern int firstBitFromRight(std::bitset<numberIntSize> binary);

extern bool MillerRabin(int power);

extern bool isLikelyPrime(int workitem);