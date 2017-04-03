#pragma once
#pragma warning( push )
#pragma warning( disable: 4146 )
#pragma warning( disable: 4800 )
#include "gmp.h"
#pragma warning( pop )
#include "threadpool.h"
//#include "commandparser.h"


struct FreeMpz
{
    void operator()(__mpz_struct* ptr) const
    {
        mpz_clear(ptr);
        delete ptr;
    }
};
using unique_mpz = std::unique_ptr<__mpz_struct,FreeMpz>;

unique_mpz GenerateRandomOdd(unsigned int Bits, unsigned int Seed);

// Forward Declarations
class NetworkController;

class Primebot
{
private:
    NetworkController* Controller;
    Threadpool<unique_mpz> tp;
    //AllPrimebotSettings Settings;
    void FindPrime(decltype(tp)& pool, unique_mpz&& workitem);
    void FoundPrime(decltype(tp)& pool, unique_mpz&& result);
public:
    Primebot() = delete;
    Primebot(unsigned int ThreadCount, NetworkController* NetController);
    ~Primebot();
    void Start();
    void Stop();
};
