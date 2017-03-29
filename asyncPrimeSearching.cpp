#include "asyncPrimeSearching.h"

#include <iterator>
#include<algorithm>
#include<future>
#include<utility>

#include "PrimeTest.h"

std::vector<int> merge(std::vector<int> first, std::vector<int> second) {
	std::vector<int> merged;
	std::merge(std::begin(first), std::end(first), std::begin(second), std::end(second), std::back_inserter(merged));
	return merged;
}

std::vector<int> congruentPrimes(int start, int step, int total) {
	std::vector<int> primes;
	for (int i = 0; i != total; i++) {
		int primeCandidate = start + i*step;
		if (isLikelyPrime(primeCandidate)) {
			primes.push_back(primeCandidate);
		}
	}
	return primes;
}

std::vector<int> findPrimes(int start, int groupSize) {
	const int threadNumber = 8;
	auto primesOne = std::async(congruentPrimes, start, threadNumber, groupSize);
	auto primesTwo = std::async(congruentPrimes, start + 2, threadNumber, groupSize);
	auto primesThree = std::async(congruentPrimes, start + 4, threadNumber, groupSize);
	auto primesFour = std::async(congruentPrimes, start + 6, threadNumber, groupSize);
	auto first_half = merge(primesOne.get(), primesTwo.get());
	auto second_half = merge(primesThree.get(), primesFour.get());
	return merge(std::move(first_half), std::move(second_half));
}