#pragma once
#if defined(_WIN32) || defined(_WIN64)
#pragma warning( push )
#pragma warning( disable: 4146 )
#pragma warning( disable: 4800 )
#if _MSC_VER >= 1900
#define __GMPXX_USE_CXX11 1
#endif
#endif
#include "gmp.h"
#include "gmpxx.h"
#if defined(_WIN32) || defined(_WIN64)
#pragma warning( pop )
#undef __GMPXX_USE_CXX11
#endif
#include <vector>
#include <memory>

struct FreeMpz
{
    void operator()(__mpz_struct* ptr) const
    {
        mpz_clear(ptr);
        delete ptr;
    }
};
using unique_mpz = std::unique_ptr<__mpz_struct, FreeMpz>;

using mpz_list = std::vector<mpz_class>;

using mpz_list_list = std::vector<mpz_list>;
