#pragma once
#include<climits>
#include<bitset>


int mod(int a, int b);

int modTimes(int a, int b, int modulus);

const int numberIntSize = sizeof(int)*CHAR_BIT;
int firstBitFromLeft(std::bitset<numberIntSize> binary);

int firstBitFromRight(std::bitset<numberIntSize> binary);

bool MillerRabin(int power);

bool isLikelyPrime(int workitem);