#pragma once

#include<vector>
#include<algorithm>
#include<numeric>
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
inline
std::vector<T> concatenate(std::vector<T> first, std::vector<T> second) {
    first.insert(std::end(first), 
        std::make_move_iterator(std::begin(second)), 
    	std::make_move_iterator(std::end(second)) 
    );
    return first;
}

//Takes elements [x1, x2, x3, x4] to [g(x1, x2), g(x2, x3), g(x3, x4)]
template<typename T, typename G>
inline
auto applyPairwise(G g, std::vector<T> elements) {
    std::vector<decltype(g(elements[0], elements[0]))> results;
    for (int i = 0; i != (elements.size()-1); i++) {
        results.push_back(g(elements[i], elements[i + 1]));
    }
    return results;
}

template<typename T>
inline
std::vector<T> partitionBorders(int numberOfPartitions, T start, T finish) {
    std::vector<T> results;
    T total = finish - start;
    results.push_back(start);
    for (int i = 0; i != numberOfPartitions; ++i) {
        results.push_back(results[i] + (total + i)/numberOfPartitions);
    }
    return results;
}

template<typename T, typename Unaryop>
inline
auto map(Unaryop op, std::vector<T> in){
    std::vector<decltype(op(std::move(in[0])))> results;
    for (int i = 0; i != in.size(); i++)
        results.push_back(op(std::move(in[i])));
    return results;
}

template<typename T, typename S, typename BinaryOp>
inline
auto accumulate(BinaryOp op, S init, std::vector<T> factors) {
    return std::accumulate(std::begin(factors), std::end(factors), init, op);
}

template<typename T>
inline
std::vector<T> primesInRange(T start, T finish) {
    std::vector<T> primes;
    if (start % 2 == 0) start++;
    for (T primeCandidate = start; primeCandidate < finish; primeCandidate = primeCandidate + 2) {
        if (isLikelyPrime(primeCandidate)) {
            primes.push_back(primeCandidate);
        }
    }
    return primes;
}

template<typename T>
inline
std::vector<T> findPrimes(T start, T finish) {
    int threadTotal = std::thread::hardware_concurrency();
    if (threadTotal == 0) return std::vector<T>{};
    auto calculatePartition = [](T start, T end) {return std::async(primesInRange<T>, start, end); };
    return 
        accumulate( concatenate<T>, std::vector<T>{},
            map( [](auto x) {return x.get(); },
                applyPairwise( calculatePartition,
                    partitionBorders( threadTotal,
                        start, 
                        finish
                    )    
                )
            )
        );
}
