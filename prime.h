#pragma once
#include "commandparsertypes.h"
#if defined(_WIN32) || defined(_WIN64)
#pragma warning( push )
#pragma warning( disable: 4146 )
#pragma warning( disable: 4800 )
#endif
#include "gmp.h"
#include "gmpxx.h"
#if defined(_WIN32) || defined(_WIN64)
#pragma warning( pop )
#endif
#include "threadpool.h"
#include <list>


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



// Forward Declarations
class NetworkController;

class Primebot
{
private:
    NetworkController* Controller;
    AllPrimebotSettings Settings;
    std::vector<std::thread> Threads;
    Threadpool<std::unique_ptr<mpz_list_list>, std::list<std::unique_ptr<mpz_list_list>>> IoPool;
    std::atomic<bool> Quit;
    std::atomic<mpz_list_list*> Results;
    std::atomic<uint64_t> ResultsCount;

    std::mutex DoneLock;
    // Used to signal that the client can shutdown
    std::condition_variable Done;

    void FindPrime(mpz_class&& workitem, int id, unsigned int BatchSize);

    void ProcessOrReportResults(std::vector<mpz_class>& Results);

    void ProcessIo(std::unique_ptr<mpz_list_list> && data);
public:
    Primebot() = delete;
    Primebot(AllPrimebotSettings config, NetworkController* NetController);
    ~Primebot();

    static std::pair<mpz_class, int> GenerateRandomOdd(unsigned int Bits, unsigned int Seed);
    static std::vector<int> DecomposeToPowersOfTwo(mpz_class prime);

    void Start();
    void Stop();
    void WaitForStop();
};
