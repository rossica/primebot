#pragma once
#include <vector>
#include <algorithm>
#include <numeric>

#include <iostream>
#include <memory>
#include <cstdlib>

#include <iterator>
#include <algorithm>
#include <future>
#include <utility>

template<typename T, typename S, typename BinaryOp>
auto accumulate(BinaryOp op, S init, std::vector<T> factors) {
    return
        std::accumulate(
            std::make_move_iterator( std::begin(factors) ),
            std::make_move_iterator( std::end(factors) ),
            init,
            op
        );
}

template<typename T>
std::vector<T> concatenate(std::vector<T> first, std::vector<T> second) {
    first.insert(std::end(first),
        std::make_move_iterator(std::begin(second)),
        std::make_move_iterator(std::end(second))
    );
    return first;
}

template<typename T>
std::vector<T> flatten(std::vector<std::vector<T>> listOfLists) {
    return accumulate(concatenate<T>, std::vector<T>{}, listOfLists);
}

template<typename T, typename Unaryop>
auto map(Unaryop op, std::vector<T> in) {
    std::vector<decltype(op(std::move(in[0])))> results;
    std::transform(
        std::make_move_iterator(std::begin(in)),
        std::make_move_iterator(std::end(in)),
        std::back_inserter(results),
        op
    );
    return results;
}

//Takes elements [x1, x2, x3, x4] to [g(x1, x2), g(x2, x3), g(x3, x4)]
template<typename T, typename G>
auto applyPairwise(G g, std::vector<T> elements) {
    std::vector<decltype(g(elements[0], elements[0]))> results;
    for (size_t i = 0; i < (elements.size() - 1); i++) {
        results.push_back(g(elements[i], elements[i + 1]));
    }
    return results;
}
