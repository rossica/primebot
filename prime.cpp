#include <iostream>
#include "prime.h"
#include "PrimeTest.h"
#include "networkcontroller.h"
#include "asyncPrimeSearching.h"
#pragma warning( push )
#pragma warning( disable: 4800 )
#include "gmpxx.h"
#pragma warning( pop )

// Yeah this is kind of ugly, Old-C style.
// Feel free to send a PR with a better way to do this.
// Maybe a Generator?
//static gmp_randstate_t Rand;
//static bool InitializedRand = false;
//static unsigned int LastSeed = 0;

unique_mpz GenerateRandomOdd(unsigned int Bits/* = 512*/, unsigned int Seed)// = LastSeed)
{
    //if (!InitializedRand)
    //{
    //    gmp_randinit_mt(Rand);
    //}

    //if (LastSeed != Seed)
    //{
    //    gmp_randseed_ui(Rand, Seed);
    //    LastSeed = Seed;
    //}

    unique_mpz Work(new __mpz_struct);
    //mpz_init_set_ui(Work.get(), 1);
    mpz_init_set_ui(Work.get(), 1);
    //mpz_urandomb(Work.get(), Rand, Bits);

    //if (mpz_even_p(Work.get()))
    //{
    //    mpz_add_ui(Work.get(), Work.get(), 1);
    //}

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
        gmp_printf("%Zd\n", result.get());
    }

    mpz_add_ui(result.get(), result.get(), (2 * pool.GetThreadCount()));
    pool.EnqueueWorkItem(std::move(result));
}

Primebot::Primebot(unsigned int ThreadCount, NetworkController* NetController) :
    Controller(NetController),
    tp(ThreadCount,
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
        // generate a large, random, odd number
        // and assign it to Start;
    }

    for (unsigned int i = 1; i <= tp.GetThreadCount(); i++)
    {
        unique_mpz work(new __mpz_struct);
        // ((2 * i) + Start)
        mpz_init_set(work.get(), Start.get());
        mpz_add_ui(work.get(), work.get(), (2 * i));
        tp.EnqueueWorkItem(std::move(work));
    }

    // Async implementation
    //mpz_class foo(Start.get());
    //mpz_class bar(Start.get());
    //bar += 10000000;
    //auto Results = findPrimes(foo, bar);

    //for (auto res : Results)
    //{
    //    Controller->ReportWork(*res.get_mpz_t());
    //}
}

void Primebot::Stop()
{
    tp.Stop();
    if (Controller != nullptr)
    {
        Controller->UnregisterClient();
    }
}

