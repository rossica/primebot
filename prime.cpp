#include "prime.h"

#include <iostream>
#include <bitset>
#include <algorithm>
#include <random>

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

const int numberIntSize = sizeof(int)*CHAR_BIT;
int firstBitFromLeft(std::bitset<numberIntSize> binary) { 
    for (auto i = binary.size()-1; i != -1; i--) {
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
    auto die = std::bind(std::uniform_int_distribution<>{1, power-1}, std::default_random_engine{});
    int x = die();
    std::bitset < numberIntSize > binary(power-1);

    int r = firstBitFromRight(binary);

    int temp = 1;
    int i = firstBitFromLeft(binary);

    for (; i != -1; i--) {
        if (binary[i] == true) {
            temp = modTimes( modTimes(x, temp, power), temp, power);
            if (i == r && (temp == 1 || temp == mod(-1, power) ) ) return true;
        }
        else {
            temp = modTimes(temp, temp, power);
            if (r > i && temp == mod(-1, power) ) return true;
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


void Primebot::FindPrime(ThreadContext<unique_mpz>& pool, unique_mpz workitem)
{
    // Implementation using GMP
    //if (mpz_cmp_ui(workitem.get(), 1000) < 0)
    //{
    //    if (mpz_probab_prime_p(workitem.get(), 20) > 0)
    //    {
    //        pool.EnqueueResult(std::move(workitem));
    //    }
    //    else
    //    {
    //        mpz_add_ui(workitem.get(), workitem.get(), 2 * pool.Pool.GetThreadCount());
    //        pool.EnqueueWork(std::move(workitem));
    //    }
    //}
    //else
    //{
    //    mpz_clear(workitem.get());
    //}

    if (*workitem < 10000)
    {
        if (isLikelyPrime(*workitem))
        {
            pool.EnqueueResult(std::move(workitem));
        }
        else
        {
            *workitem = *workitem + (2 * pool.Pool.GetThreadCount());
            pool.EnqueueWork(std::move(workitem));
        }
    }
    else
    {
    }
}

void Primebot::FoundPrime(ThreadContext<unique_mpz>& pool, unique_mpz result)
{
    // Implementation using GMP
    //gmp_printf("%Zd is prime!\n", result);
    //mpz_add_ui(result.get(), result.get(), 2 * pool.Pool.GetThreadCount());
    std::cout << *result << " is prime!" << std::endl;
    *result = *result + (2 * pool.Pool.GetThreadCount());
    pool.EnqueueWork(std::move(result));
}

Primebot::Primebot(unsigned int ThreadCount, void * NetController) :
    tp(ThreadCount,
        std::bind(&Primebot::FindPrime, this, std::placeholders::_1, std::placeholders::_2),
        std::bind(&Primebot::FoundPrime, this, std::placeholders::_1, std::placeholders::_2))
{
    if (NetController)
    {
        NetworkController = NetController;
    }
    else
    {
        NetworkController = nullptr;
    }
}

Primebot::~Primebot()
{
}

void Primebot::Start()
{
    // generate a large, random, odd number
    // add it to the threadpool
    // add 2
    // add it to the threadpool
    // repeat

    for (int i = 1; i <= tp.GetThreadCount(); i++)
    {
        // implementation using GMP
        //unique_mpz foo(new __mpz_struct);
        //mpz_init(foo.get());
        //mpz_set_ui(foo.get(), (2 * i) + 1);
        //tp.EnqueueWorkItem(std::move(foo));

        tp.EnqueueWorkItem(std::make_unique<int>((2 * i) + 1));
    }
}

