#pragma once
#include "commandparsertypes.h"
#include "gmpwrapper.h"
#include "threadpool.h"
#include "fileio.h"
#include <list>


// Forward Declarations
class NetworkController;

class Primebot
{
private:
    NetworkController* Controller;
    AllPrimebotSettings Settings;
    PrimeFileIo FileIo;
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
    static mpz_class GetInitialWorkitem(AllPrimebotSettings& Settings);

    void Start();
    void Stop();
    void WaitForStop();
};
