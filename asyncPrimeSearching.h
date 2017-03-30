#pragma once

#include<vector>
#include<algorithm>
#pragma warning( push )
#pragma warning( disable: 4146 )
#include "gmp.h"
#include "gmpxx.h"
#pragma warning( pop )
#include<iostream>
#include <memory>
#include <cstdlib>

#include<iterator>
#include<algorithm>
#include<future>
#include<utility>

#include "PrimeTest.h"

template<typename T>
std::vector<T> merge(std::vector<T> first, std::vector<T> second){
	std::vector<T> merged;
	std::merge(std::begin(first), std::end(first), std::begin(second), std::end(second), std::back_inserter(merged));
	return merged;
}

template<typename T>
std::vector<T> congruentPrimes(T start, T step, T total) {
	std::vector<T> primes;
	for (T i = 0; i != total; i++) {
		T primeCandidate = start + i*step;
		if (isLikelyPrime(primeCandidate)) {
			primes.push_back(primeCandidate);
		}
	}
	return primes;
}

template<typename T>
std::vector<T> findPrimes(T start, T groupSize) {
	const T threadNumber = 8;
	auto primesOne = std::async(congruentPrimes<T>, start, threadNumber, groupSize);
	auto primesTwo = std::async(congruentPrimes<T>, start + 2, threadNumber, groupSize);
	auto primesThree = std::async(congruentPrimes<T>, start + 4, threadNumber, groupSize);
	auto primesFour = std::async(congruentPrimes<T>, start + 6, threadNumber, groupSize);
	auto first_half = merge(primesOne.get(), primesTwo.get());
	auto second_half = merge(primesThree.get(), primesFour.get());
	return merge(std::move(first_half), std::move(second_half));
}
