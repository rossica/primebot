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

bool isEven(mpz_class test) {
    return mpz_even_p(test.get_mpz_t());
}

struct primesInRange {
    std::vector<mpz_class> operator()(mpz_class start, mpz_class finish) {
        std::vector<mpz_class> primes;
        if (isEven(start)) start++;
 
        for (auto primeCandidate = start; primeCandidate < finish; primeCandidate += 2) {
            if (isLikelyPrime(primeCandidate)) {
                primes.push_back(primeCandidate);
            }
        }
        return primes;
    }
};


std::vector<mpz_class> gatherResultsFromThreads(
    std::vector<
        std::future< std::vector<mpz_class> > 
    > in) {
    return 
        accumulate(concatenate<mpz_class>, std::vector<mpz_class>{},
            map([](auto x) { return x.get(); },
                std::move(in)
            )
        );
}

std::vector<
    std::future< std::vector<mpz_class> > 
> 
asyncronouslyDistributePrimeTest(int threadTotal, mpz_class start, mpz_class finish) {
    auto calculatePartition = [](mpz_class start, mpz_class end) {
        return std::async(std::launch::async, primesInRange{}, start, end);
    };
    return
        applyPairwise(calculatePartition,
            partitionBorders(threadTotal,
                start,
                finish
            )
        );
}

std::vector<mpz_class> findPrimes(mpz_class start, mpz_class finish) {  
    int threadTotal = std::thread::hardware_concurrency();
    if (threadTotal == 0) return std::vector<mpz_class>{};

    return  
        gatherResultsFromThreads( 
            asyncronouslyDistributePrimeTest(
                threadTotal, 
                start, 
                finish
            )
        );        
}