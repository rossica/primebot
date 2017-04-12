#include <iostream>
#include "prime.h"
#include "networkcontroller.h"
#include "asyncPrimeSearching.h" 
#include "primeSearchingUtilities.h"
#include "fileio.h"

std::atomic<int> Primebot::RandomIterations(0);


// Yeah this is kind of ugly, Old-C style.
// Feel free to send a PR with a better way to do this.
// Maybe a Generator?
std::pair<mpz_class, int> Primebot::GenerateRandomOdd(unsigned int Bits, unsigned int Seed)
{
    static gmp_randstate_t Rand = { 0 };
    static unsigned int LastSeed = 0;
    int PreviousIteration;
    if (Rand[0]._mp_algdata._mp_lc == nullptr) // does this even work?
    {
        gmp_randinit_mt(Rand);
    }

    if (LastSeed != Seed)
    {
        gmp_randseed_ui(Rand, Seed);
        LastSeed = Seed;
        RandomIterations = 0;
    }

    mpz_class Work;
    //mpz_init_set_ui(Work.get(), 1);
    mpz_urandomb(Work.get_mpz_t(), Rand, Bits);

    PreviousIteration = RandomIterations.fetch_add(1);

    if (mpz_even_p(Work.get_mpz_t()))
    {
        Work += 1;
    }

    return std::make_pair(std::move(Work), PreviousIteration + 1);
}

std::vector<int> Primebot::DecomposeToPowersOfTwo(mpz_class Input)
{
    // Convert the prime into a nicer list of powers of two?
    mpz_class Candidate(Input);
    std::vector<int> Powers;
    unsigned int Base = 2;
    while (mpz_cmp_ui(Candidate.get_mpz_t(), 1) >= 0)
    {
        unsigned int Exp = mpz_sizeinbase(Candidate.get_mpz_t(), Base);
        mpz_class Test(Base);
        mpz_pow_ui(Test.get_mpz_t(), Test.get_mpz_t(), Exp);

        while (mpz_cmp(Test.get_mpz_t(), Candidate.get_mpz_t()) > 0)
        {
            // if the test power is greater, decrement Exp
            // and regenerate Test;
            Test = Base;
            Exp--;
            mpz_pow_ui(Test.get_mpz_t(), Test.get_mpz_t(), Exp);
        }

        Candidate -= Test;
        Powers.push_back(Exp);
    }
    return Powers;
}

void Primebot::FindPrime(mpz_class && workitem)
{
    unsigned int Step = 2 * Settings.PrimeSettings.ThreadCount;

    for (unsigned long long j = 0; j < Settings.PrimeSettings.BatchCount && !Quit; j++)
    {
        std::vector<mpz_class> Batch;

        for (unsigned int i = 0; i < Settings.PrimeSettings.BatchSize && !Quit; i++)
        {
            if (mpz_even_p(workitem.get_mpz_t()))
            {
                std::cout << "WHY IS THIS EVEN?!" << std::endl;
                workitem += 1;
            }

            // Miller-Rabin has 4^-n (or (1/4)^n if you prefer) probability that a
            // composite number will pass the nth iteration of the test.
            // The below selects the bit-length of the candidate prime, divided by 2
            // as the number of iterations to perform.
            // This has the property of putting the probability of a composite passing
            // the test to be less than the count of possible numbers for a given bit-length.
            // Or so I think...
            while (mpz_probab_prime_p(workitem.get_mpz_t(), (mpz_sizeinbase(workitem.get_mpz_t(), 2) / 2)) == 0 && !Quit)
            {
                workitem += Step;
            }

            // If not quitting, just add the number to the list of results
            if (!Quit)
            {
                // Copy the result to the Results queue
                mpz_class Result(workitem);
                Batch.push_back(std::move(Result));
            }
            else
            {
                // During quit, make sure that the number is actually prime
                if (mpz_probab_prime_p(workitem.get_mpz_t(), (mpz_sizeinbase(workitem.get_mpz_t(), 2) / 2)))
                {
                    // Copy the result to the Results queue
                    mpz_class Result(workitem);
                    Batch.push_back(std::move(Result));
                }
            }

            workitem += Step;
        }

        ProcessOrReportResults(Batch);

        Batch.clear();
    }
}

void Primebot::ProcessOrReportResults(std::vector<mpz_class>& Results)
{
    // Use network to report results
    if (Controller != nullptr)
    {
        Controller->BatchReportWork(Results);
    }
    else
    {
        for (auto& res : Results)
        {
            if (Settings.FileSettings.Path.empty())
            {
                // print results to console
                gmp_printf("%Zd\n", res.get_mpz_t());
            }
            else
            {
                // Write out to disk here if file path present
                WritePrimeToFile(
                    Settings.FileSettings.Path,
                    res.get_str(STRING_BASE));
            }
        }
    }
}

Primebot::Primebot(AllPrimebotSettings Config, NetworkController* NetController) :
    Controller(NetController),
    Settings(Config),
    Threads(Config.PrimeSettings.ThreadCount),
    Quit(false)
{
}

Primebot::~Primebot()
{
    Stop();
}

void Primebot::Start()
{
    mpz_class Start;

    if (Controller != nullptr)
    {
        // Start listening for events from the server
        Controller->Start();

        while (!Controller->RegisterClient() & !Quit)
        {
            // Failed to register client, try again
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        Start = Controller->RequestWork();
        while ((Start.get_ui() == 0) & !Quit)
        {
            // Failed to get work item, try again.
            std::this_thread::sleep_for(std::chrono::seconds(1));
            Start = Controller->RequestWork();
        }
    }
    else
    {
        // Stand-alone mode, generate starting prime
        auto RandomWork = GenerateRandomOdd(Settings.PrimeSettings.Bitsize, Settings.PrimeSettings.RngSeed);
        Start = RandomWork.first;

        // Update the path setting only if it's specified.
        if (!Settings.FileSettings.Path.empty())
        {
            Settings.FileSettings.Path = GetPrimeBasePath(Settings, RandomWork.second);
        }
    }

    if (Settings.PrimeSettings.UseAsync)
    {
        // Async implementation
        mpz_class AsyncStart(Start);
        mpz_class AsyncEnd(AsyncStart);
        unsigned int RangeSize = Settings.PrimeSettings.BatchSize * Settings.PrimeSettings.ThreadCount;

        for (unsigned long long i = 0; (i < Settings.PrimeSettings.BatchCount) & !Quit; i++)
        {
            std::vector<mpz_class> Results;
            while ((Results.size() < RangeSize) & !Quit)
            {
                // Make the range grow proportionally to the size of numbers being searched.
                AsyncEnd += 
                    RangeSize
                    * (unsigned int) mpz_sizeinbase(AsyncStart.get_mpz_t(), 2);

                Results = concatenate(
                    Results,
                    findPrimes(Settings.PrimeSettings.ThreadCount, AsyncStart, AsyncEnd));

                // Move the start to the end of the range
                AsyncStart = AsyncEnd;
            }

            ProcessOrReportResults(Results);
        }
    }
    else
    {
        for (unsigned int i = 1; i <= Threads.size(); i++)
        {
            // ((2 * i) + Start)
            mpz_class work(Start);
            work += (2 * i);
            Threads[i-1] = std::move(std::thread(&Primebot::FindPrime, this, std::move(work)));
            //Candidates.EnqueueWorkItem(std::move(work));
        }
    }
}

void Primebot::Stop()
{
    if (!Quit)
    {
        // Stop running loops
        Quit = true;
        // Wait for threads to finish
        for (auto& t : Threads)
        {
            if (t.joinable())
            {
                t.join();
            }
        }
        Threads.clear();
        // Signal to server that we're done
        if (Controller != nullptr)
        {
            Controller->UnregisterClient();
        }
        // Notify main() that it's time to quit
        {
            std::unique_lock<std::mutex> lock(DoneLock);
            Done.notify_all();
        }
    }
}

void Primebot::WaitForStop()
{
    if (Settings.PrimeSettings.UseAsync)
    {
        //int dummy = 0;
        //std::cout << "Type anything and press enter to exit.";
        //std::cin >> dummy;

        Stop();
    }
    else
    {
        std::unique_lock<std::mutex> lock(DoneLock);

        // Wait to exit
        Done.wait(lock);
    }
}

