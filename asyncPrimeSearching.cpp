#include "asyncPrimeSearching.h"
#include "primeSearchingUtilities.h"

#include "PrimeTest.h"

std::vector<mpz_class> partitionBorders(int numberOfPartitions, mpz_class start, mpz_class finish) {
    std::vector<mpz_class> results;
    mpz_class total = finish - start;
    results.push_back(start);
    for (int i = 0; i != numberOfPartitions; ++i) {
        results.push_back(results[i] + (total + i) / numberOfPartitions);
    }
    return results;
}

struct primesInRange {
    std::vector<mpz_class> operator()(mpz_class start, mpz_class finish) {
        std::vector<mpz_class> primes;
        if (start % 2 == 0) start++;
        for (auto primeCandidate = start; primeCandidate < finish; primeCandidate = primeCandidate + 2) {
            if (isLikelyPrime(primeCandidate)) {
                primes.push_back(primeCandidate);
            }
        }
        return primes;
    }
};

std::vector<std::string> findPrimes(std::string start, std::string finish) {
  
    int threadTotal = std::thread::hardware_concurrency();
    if (threadTotal == 0) return std::vector<std::string>{};
    auto calculatePartition = [](mpz_class start, mpz_class end) {
        return std::async(primesInRange{}, start, end);
    };
    return
        map( [](auto x) { return x.get_str(); },
            accumulate(concatenate<mpz_class>, std::vector<mpz_class>{},
                map([](auto x) { return x.get(); },
                    applyPairwise(calculatePartition,
                        partitionBorders(threadTotal,
                            mpz_class{ start },
                            mpz_class{ finish }
                        )
                    )
                )
            )
        );
}

std::string multiprecision_add(std::string first, std::string second) {
    mpz_class x{ first };
    mpz_class y{ second };
    mpz_class z = x + y;
    return z.get_str();
}

std::string multiprecision_multiply(std::string first, std::string second) {
    mpz_class x{ first };
    mpz_class y{ second };
    mpz_class z = x * y;
    return z.get_str();
}