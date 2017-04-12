#include "numberSet.h"
#include <cmath>
#include <deque>

#include "primeSearchingUtilities.h"

//Most significant figure starts at 0.
int from_digits(std::vector<int> number, int base) {
    int out_number = 0;
    int coordinate = 1;
    for (int i = number.size()-1; i >= 0; --i) {
        out_number += number[i]*coordinate;
        coordinate *= 10;
    }
    return out_number;
}

std::vector<int> to_vector(numberSet ns) {
    std::vector<int> number_stack{};
    return to_vector_impl(ns, number_stack);
}

std::vector<int> to_vector_impl(const numberSet& ns, std::vector<int> current_head) {
    std::vector<int> result;
    
    if (ns.inSet) result.push_back(from_digits(current_head, base));
   
    std::vector<std::vector<int>> numbersFromSets{};
    for (int i = 0; i != ns.branches.size(); i++) {
        if (ns.branches[i] != nullptr) {
            numbersFromSets.push_back(
                to_vector_impl(*ns.branches[i], concatenate(current_head, std::vector<int>{i}))
            );
        }
    }
    
    return 
        concatenate(
            result,
            flatten(numbersFromSets)
        );
}

std::deque<int> digit_sequence(int member) {
    std::deque<int> digits;
    while (member > 0) {
        int digit = member % base;
        digits.push_front(digit);
        member /= 10;
    }
    return digits;
}

numberSet::numberSet(const numberSet& rhs) :inSet{ rhs.inSet } {
    for (int i = 0; i != branches.size(); ++i) {
        if (rhs.branches[i] == nullptr) {
            branches[i] = nullptr;
        }
        else {
            auto branch = std::unique_ptr<numberSet>{ new numberSet{*rhs.branches[i]} };
            branches[i] = std::move(branch);
        }
    }
}
numberSet& numberSet::operator=(const numberSet& rhs) {
    numberSet tmp{ rhs };
    std::swap(tmp, *this);
    return *this;
}

//add number to set
numberSet with(numberSet set, int member) {
    
    auto digits = digit_sequence(member);

    numberSet* setFocus = &set;
    for (auto digit : digits) {
        if (setFocus->branches[digit] != nullptr) {
            setFocus = setFocus->branches[digit].get();
        }
        else {
            setFocus->branches[digit] = std::unique_ptr<numberSet>(new numberSet{});
            setFocus = setFocus->branches[digit].get();
        }
    }
    setFocus->inSet = true;

    return set;
}

bool has(const numberSet& set, int member) {
    auto digits = digit_sequence(member);
    const numberSet* setFocus = &set;
    for (auto digit : digits) {
        if (setFocus->branches[digit] != nullptr) {
            setFocus = setFocus->branches[digit].get();
        }
        else {
            return false;
        }
    }

    return setFocus->inSet;
}
