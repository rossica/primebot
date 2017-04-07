#pragma once
#include "commandparsertypes.h"
#pragma warning( push )
#pragma warning( disable: 4146 )
#pragma warning( disable: 4800 )
#include "gmp.h"
#include "gmpxx.h"
#pragma warning( pop )
#include "threadpool.h"



struct FreeMpz
{
    void operator()(__mpz_struct* ptr) const
    {
        mpz_clear(ptr);
        delete ptr;
    }
};
using unique_mpz = std::unique_ptr<__mpz_struct, FreeMpz>;



// Forward Declarations
class NetworkController;

class Primebot
{
private:
    NetworkController* Controller;
    Threadpool<unique_mpz> Candidates;
    AllPrimebotSettings Settings;
    void FindPrime(decltype(Candidates)& pool, unique_mpz&& workitem);
    std::atomic<bool> Quit;
public:
    Primebot() = delete;
    Primebot(AllPrimebotSettings config, NetworkController* NetController);
    ~Primebot();

    static unique_mpz GenerateRandomOdd(unsigned int Bits, unsigned int Seed);

    void Start();
    void Stop();
};
