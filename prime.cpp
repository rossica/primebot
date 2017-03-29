#include "prime.h"
#include "PrimeTest.h"
#include "networkcontroller.h"

#include <iostream>


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

    if (*workitem < 100000)
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

    if (Controller != nullptr)
    {
        Controller->ReportWork(*result);
    }
    else
    {
        std::cout << *result << std::endl;
    }

    *result = *result + (2 * pool.Pool.GetThreadCount());
    pool.EnqueueWork(std::move(result));
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
    if (Controller != nullptr)
    {
        Controller->RegisterClient();
        int Start = Controller->RequestWork();
        for (int i = 1; i <= tp.GetThreadCount(); i++)
        {
            tp.EnqueueWorkItem(std::make_unique<int>((2 * i) + Start));
        }
    }
    else
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
}

void Primebot::Stop()
{
    tp.Stop();
    if (Controller != nullptr)
    {
        Controller->UnregisterClient();
    }
}

