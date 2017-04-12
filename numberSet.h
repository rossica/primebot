#pragma once
#include <array>
#include <memory>
#include "asyncPrimeSearching.h"
#include <vector>

const int base = 10;
class numberSet {
public:
    numberSet() = default;

    numberSet(const numberSet&);
    numberSet& operator=(const numberSet&);

    numberSet(numberSet&&) = default;
    numberSet& operator=(numberSet&&) = default;

    ~numberSet() = default; //TODO: double check destruction works properly.

    friend numberSet with(numberSet, int);
    friend bool has(const numberSet&, int);
    friend std::vector<int> to_vector(numberSet);
private:
    friend std::vector<int> to_vector_impl(const numberSet& ns, std::vector<int> current_head);

    std::array<std::unique_ptr<numberSet>, base> branches{};
    bool inSet = false;
};