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
    Threadpool<mpz_class> Candidates;
    AllPrimebotSettings Settings;
    void FindPrime(decltype(Candidates)& pool, mpz_class&& workitem);
    std::atomic<bool> Quit;
    static std::atomic<int> RandomIterations;

    void ProcessOrReportResults(std::vector<mpz_class>& Results);
public:
    Primebot() = delete;
    Primebot(AllPrimebotSettings config, NetworkController* NetController);
    ~Primebot();

    static std::pair<mpz_class, int> GenerateRandomOdd(unsigned int Bits, unsigned int Seed);
    static std::vector<int> DecomposeToPowersOfTwo(mpz_class prime);

    void Start();
    void Stop();
};
