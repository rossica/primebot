#pragma once
#pragma warning( push )
#pragma warning( disable: 4146 )
#include "gmp.h"
#pragma warning( pop )
#include "onelockthreadpool.h"

// uncomment to use GMP
//using unique_mpz = std::unique_ptr<__mpz_struct>;
using unique_mpz = std::unique_ptr<int>;

// Forward Declarations
class NetworkController;

class Primebot
{
private:
    NetworkController* Controller;
    void FindPrime(ThreadContext<unique_mpz>& pool, unique_mpz workitem);
    void FoundPrime(ThreadContext<unique_mpz>& pool, unique_mpz result);
    OneLockThreadpool<unique_mpz> tp;
public:
    Primebot() = delete;
    Primebot(unsigned int ThreadCount, NetworkController* NetController);
    ~Primebot();
    void Start();
    void Stop();
};
