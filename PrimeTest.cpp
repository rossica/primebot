#include "PrimeTest.h"

#include <bitset>
#include <algorithm>
#include <random>
#include <functional>

int mod(int a, int b)
{
	if (b < 0)
		return mod(a, -b);
	int ret = a % b;
	if (ret < 0)
		ret += b;
	return ret;
}

int modTimes(int a, int b, int modulus) {
	return mod(a*b, modulus);
}

int firstBitFromLeft(std::bitset<numberIntSize> binary) {
	for (auto i = binary.size() - 1; i != -1; i--) {
		if (binary[i] == true) return static_cast<int>(i);
	}
	return -1;
}

int firstBitFromRight(std::bitset<numberIntSize> binary) {
	for (int i = 0; i != binary.size(); i++) {
		if (binary[i] == true) return static_cast<int>(i);
	}
	return -1;
}

bool MillerRabin(int power) {
	auto die = std::bind(std::uniform_int_distribution<>{1, power - 1}, std::default_random_engine{});
	int x = die();
	std::bitset < numberIntSize > binary(power - 1);

	int r = firstBitFromRight(binary);

	int temp = 1;
	int i = firstBitFromLeft(binary);

	for (; i != -1; i--) {
		if (binary[i] == true) {
			temp = modTimes(modTimes(x, temp, power), temp, power);
			if (i == r && (temp == 1 || temp == mod(-1, power))) return true;
		}
		else {
			temp = modTimes(temp, temp, power);
			if (r > i) {
				if (temp == mod(-1, power))
					return true;
				else if (temp == mod(1, power))
					return false;
			}
		}
	}
	return false;
}

bool isLikelyPrime(int workitem) {
	bool passed = true;
	int i = 20;
	while (i--) {
		passed = (passed && MillerRabin(workitem));
	}
	return passed;
}

bool isLikelyPrime(mpz_class workitem) {
	return mpz_probab_prime_p(
        workitem.get_mpz_t(), 
        25
    );
}
