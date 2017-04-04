#include <iostream>
#include "prime.h"
#include "networkcontroller.h"
#include "asyncPrimeSearching.h" 
#pragma warning( push )
#pragma warning( disable: 4146 )
#pragma warning( disable: 4800 )
#include "gmpxx.h" 
#pragma warning( pop )


// Yeah this is kind of ugly, Old-C style.
// Feel free to send a PR with a better way to do this.
// Maybe a Generator?
unique_mpz Primebot::GenerateRandomOdd(unsigned int Bits, unsigned int Seed)
{
    static gmp_randstate_t Rand = { 0 };
    static unsigned int LastSeed = 0;
    if (Rand[0]._mp_algdata._mp_lc == nullptr) // does this even work?
    {
        gmp_randinit_mt(Rand);
    }

    if (LastSeed != Seed)
    {
        gmp_randseed_ui(Rand, Seed);
        LastSeed = Seed;
    }

    unique_mpz Work(new __mpz_struct);
    //mpz_init_set_ui(Work.get(), 1);
    mpz_init(Work.get());
    mpz_urandomb(Work.get(), Rand, Bits);

    if (mpz_even_p(Work.get()))
    {
        mpz_add_ui(Work.get(), Work.get(), 1);
    }

    return std::move(Work);
}

void Primebot::FindPrime(decltype(tp)& pool, unique_mpz&& workitem)
{
    // Miller-Rabin has 4^-n (or (1/4)^n if you prefer) probability that a
    // composite number will pass the nth iteration of the test.
    // The below selects the bit-length of the candidate prime, divided by 2
    // as the number of iterations to perform.
    // This has the property of putting the probability of a composite passing
    // the test to be less than the count of possible numbers for a given bit-length.
    // Or so I think...
    if (mpz_probab_prime_p(workitem.get(), (mpz_sizeinbase(workitem.get(), 2)/2)))
    {
        pool.EnqueueResult(std::move(workitem));
    }
    else
    {
        mpz_add_ui(workitem.get(), workitem.get(), 2 * pool.GetThreadCount());
        pool.EnqueueWorkItem(std::move(workitem));
    }
}

void Primebot::FoundPrime(decltype(tp)& pool, unique_mpz&& result)
{
    if (Controller != nullptr)
    {
        if (!Controller->ReportWork(*result))
        {
            // Failed to send work, try again
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            pool.EnqueueWorkItem(std::move(result));
            return;
        }
    }
    else
    {
        // Todo: write out to disk here if file path present
        gmp_printf("%Zd\n", result.get());
    }

    mpz_add_ui(result.get(), result.get(), (2 * pool.GetThreadCount()));
    pool.EnqueueWorkItem(std::move(result));
}

Primebot::Primebot(AllPrimebotSettings Config, NetworkController* NetController) :
    Settings(Config),
    Controller(NetController),
    tp(Config.PrimeSettings.ThreadCount,
        std::bind(&Primebot::FindPrime, this, std::placeholders::_1, std::placeholders::_2),
        std::bind(&Primebot::FoundPrime, this, std::placeholders::_1, std::placeholders::_2))
{
}

Primebot::~Primebot()
{
}

void Primebot::Start()
{
    unique_mpz Start(nullptr);

    if (Controller != nullptr)
    {
        while (!Controller->RegisterClient())
        {
            // Failed to register client, try again
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        Start = Controller->RequestWork();
        while (Start == nullptr)
        {
            // Failed to get work item, try again.
            std::this_thread::sleep_for(std::chrono::seconds(1));
            Start = Controller->RequestWork();
        }
    }
    else
    {
        Start = GenerateRandomOdd(Settings.PrimeSettings.Bitsize, Settings.PrimeSettings.RngSeed);
    }

    if (Settings.PrimeSettings.UseAsync)
    {
        // Async implementation
        mpz_class AsyncStart(Start.get());
        mpz_class AsyncEnd(AsyncStart);
        AsyncEnd += std::thread::hardware_concurrency() * 1000;
        auto Results = findPrimes(AsyncStart, AsyncEnd);

        // Use network to report results
        if (Controller != nullptr)
        {
            for (auto res : Results)
            {
                Controller->ReportWork(*res.get_mpz_t());
            }
        }
        else // print results to console
        {
            for (auto res : Results)
            {
                gmp_printf("%Zd\n", res.get_mpz_t());
            }
        }
    }
    else
    {
        for (unsigned int i = 1; i <= tp.GetThreadCount(); i++)
        {
            unique_mpz work(new __mpz_struct);
            // ((2 * i) + Start)
            mpz_init_set(work.get(), Start.get());
            mpz_add_ui(work.get(), work.get(), (2 * i));
            tp.EnqueueWorkItem(std::move(work));
        }
    }
}

void Primebot::Stop()
{
    tp.Stop();
    if (Controller != nullptr)
    {
        Controller->UnregisterClient();
    }
}

