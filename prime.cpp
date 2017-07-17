#include <iostream>
#include <cmath>
#include <cassert>
#include "prime.h"
#include "networkcontroller.h"
#include "asyncPrimeSearching.h" 
#include "primeSearchingUtilities.h"
#include "fileio.h"

inline size_t GetReservedSize(unsigned int BatchSize, mpz_ptr Candidate)
{
    // Approximates ln(2).
    typedef std::ratio<34657, 50000> ln2;

    // ceil()s to the nearest 1000
    return std::ceil(((BatchSize) / ((mpz_sizeinbase(Candidate, 2) * ln2::num) / ln2::den)) / 1000.0) * 1000;
}


// Yeah this is kind of ugly, Old-C style.
// Feel free to send a PR with a better way to do this.
// Maybe a Generator?
std::pair<mpz_class, int> Primebot::GenerateRandomOdd(unsigned int Bits, unsigned int Seed)
{
    static gmp_randstate_t Rand = { 0 };
    static unsigned int LastSeed = 0;
    static std::atomic<int> RandomIterations(0);

    int PreviousIteration;
    if (Rand[0]._mp_algdata._mp_lc == nullptr)
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
    mpz_urandomb(Work.get_mpz_t(), Rand, Bits);

    PreviousIteration = RandomIterations.fetch_add(1);

    if (mpz_even_p(Work.get_mpz_t()))
    {
        Work += 1;
    }

    return std::make_pair(std::move(Work), PreviousIteration + 1);
}

mpz_class Primebot::GetInitialWorkitem(const AllPrimebotSettings& Settings)
{
    if (Settings.PrimeSettings.StartValue.empty())
    {
        auto RandomWork = GenerateRandomOdd(Settings.PrimeSettings.Bitsize, Settings.PrimeSettings.RngSeed);
        return RandomWork.first;
    }
    else
    {
        return mpz_class(Settings.PrimeSettings.StartValue, Settings.PrimeSettings.StartValueBase);
    }
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

void Primebot::FindPrimeStandalone(mpz_class && workitem, int id, unsigned int BatchSize)
{
    unsigned int Step = 2 * BatchSize * (Settings.PrimeSettings.ThreadCount - 1);

    for (unsigned long long j = 0; j < Settings.PrimeSettings.BatchCount && !Quit; j++)
    {
        assert(mpz_odd_p(workitem.get_mpz_t()));

        mpz_list Batch;
        // This wastes some memory, but we'll never have to do a resize operation.
        Batch.reserve(GetReservedSize(2 * BatchSize, workitem.get_mpz_t()));

        // Miller-Rabin has 4^-n (or (1/4)^n if you prefer) probability that a
        // composite number will pass the nth iteration of the test.
        // The below selects the bit-length of the candidate prime, divided by 2
        // as the number of iterations to perform.
        // This has the property of putting the probability of a composite passing
        // the test to be less than the count of possible numbers for a given bit-length.
        // Or so I think...
        int MillerRabinIterations = (mpz_sizeinbase(workitem.get_mpz_t(), 2) / 2);

        // The increment of workitem in the last iteration is essential to making
        // sure there are no gaps.
        for (unsigned int i = 0; i < BatchSize && !Quit; i++, workitem += 2)
        {
            if (mpz_probab_prime_p(workitem.get_mpz_t(), MillerRabinIterations))
            {
                // this will invoke the copy constructor
                Batch.emplace_back(workitem);
            }
        }

        // If this thread has reported any results already
        // wait until the last thread cleans up the results before inserting
        // TODO: this could be optimized somehow...
        while (Results.load()->data()[id].size())
        {
            std::this_thread::yield();
        }

        // Store results in shared array
        // After this, Batch should be empty
        Results.load()->data()[id].swap(Batch);

        // At this point, Batch should be empty
        assert(Batch.empty());

        // Create new Results list in case we're the last thread and have to
        // send results; this'll be cleaned up if unused.
        std::unique_ptr<mpz_list_list> MyResults(new mpz_list_list(Settings.PrimeSettings.ThreadCount));

        if (++ResultsCount % Settings.PrimeSettings.ThreadCount == 0)
        {
            // Swap the shared Results vector with the temp one
            MyResults.reset(Results.exchange(MyResults.release()));

            IoPool.EnqueueWorkItem(std::move(MyResults));

        }
        MyResults.reset(nullptr);

        // move to the next batch
        workitem += Step;
    }
}

void Primebot::FindPrimesClient()
{
    for (unsigned long long j = 0; j < Settings.PrimeSettings.BatchCount && !Quit; j++)
    {
        // Get workitem from server
        ClientWorkitem Workitem = Controller->RequestWork(1);
        while ((Workitem.Start.get_ui() == 0 || Workitem.Offset == 0 || Workitem.Id == 0) & !Quit)
        {
            // Failed to get work item, try again.
            std::this_thread::sleep_for(std::chrono::seconds(1));
            Workitem = Controller->RequestWork(1);
        }

        if ((Workitem.Start.get_ui() == 0 || Workitem.Offset == 0 || Workitem.Id == 0) && Quit)
        {
            // This lets clients that have just downloaded a workitem to
            // complete it and prevent the server from hanging.
            break;
        }

        mpz_list Batch;
        // This wastes some memory, but we'll never have to do a resize operation.
        Batch.reserve(GetReservedSize(Workitem.Offset, Workitem.Start.get_mpz_t()));

        // Miller-Rabin has 4^-n (or (1/4)^n if you prefer) probability that a
        // composite number will pass the nth iteration of the test.
        // The below selects the bit-length of the candidate prime, divided by 2
        // as the number of iterations to perform.
        // This has the property of putting the probability of a composite passing
        // the test to be less than the count of possible numbers for a given bit-length.
        // Or so I think...
        int MillerRabinIterations = (mpz_sizeinbase(Workitem.Start.get_mpz_t(), 2) / 2);

        // The increment of workitem in the last iteration is essential to making
        // sure there are no gaps.
        // Note: this loop wont be canceled by Quit because it's guaranteed to
        // always reach a terminal state, so it'll always return a full batch.
        for (unsigned int i = 0; i < Workitem.Offset / 2; i++, Workitem.Start += 2)
        {
            if (mpz_probab_prime_p(Workitem.Start.get_mpz_t(), MillerRabinIterations))
            {
                Batch.emplace_back(Workitem.Start);
            }
        }

        Controller->ReportWork(Workitem.Id, Batch);
    }
    // If the batch count is reached, call stop()
    std::thread(&Primebot::Stop, this).detach();
}

void Primebot::ProcessOrReportResults(std::vector<mpz_class>& Results)
{
    // Don't use network to report results
    if (Controller == nullptr)
    {
        if (Settings.FileSettings.Path.empty())
        {
            for (auto& res : Results)
            {
                // print results to console
                gmp_fprintf(stdout, "%Zd\n", res.get_mpz_t());
            }
        }
        else
        {
            // Write out to disk here if file path present
            FileIo.WritePrimes(Results);
        }
    }
}

// This will contend with the worker threads, but the OS thread
// scheduler should be smart enough to schedule the worker threads
// when this thread is waiting for I/O.
void Primebot::ProcessIo(std::unique_ptr<mpz_list_list> && data)
{
    for (mpz_list& l : *data)
    {
        ProcessOrReportResults(l);
    }
}

void Primebot::StartClient()
{
    // Start listening for events from the server
    Controller->Start();

    while (!Controller->RegisterClient() & !Quit)
    {
        // Failed to register client, try again
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // I/O pool unused in client mode.
    IoPool.Stop();

    if (Settings.PrimeSettings.UseAsync)
    {
        RunAsyncClient();
    }
    else
    {
        RunThreadsClient();
    }
}

void Primebot::StartStandalone()
{
    if (Settings.PrimeSettings.UseAsync)
    {
        RunAsyncStandalone();
    }
    else
    {
        RunThreadsStandalone();
    }
}

void Primebot::RunAsyncClient()
{
    for (unsigned long long i = 0; (i < Settings.PrimeSettings.BatchCount) & !Quit; i++)
    {
        // Get workitem from server
        ClientWorkitem Workitem = Controller->RequestWork(Settings.PrimeSettings.ThreadCount);
        while ((Workitem.Start.get_ui() == 0 || Workitem.Offset == 0 || Workitem.Id == 0) & !Quit)
        {
            // Failed to get work item, try again.
            std::this_thread::sleep_for(std::chrono::seconds(1));
            Workitem = Controller->RequestWork(Settings.PrimeSettings.ThreadCount);
        }

        if ((Workitem.Start.get_ui() == 0 || Workitem.Offset == 0 || Workitem.Id == 0) && Quit)
        {
            break;
        }

        // Initialize start and end for asynchronous work
        mpz_class AsyncStart(Workitem.Start);
        mpz_class AsyncEnd(AsyncStart);

        AsyncEnd += Workitem.Offset;

        Controller->ReportWork(Workitem.Id, findPrimes(Settings.PrimeSettings.ThreadCount, AsyncStart, AsyncEnd));
    }
}

void Primebot::RunAsyncStandalone()
{
    // Stand-alone mode, generate starting prime
    mpz_class Start = GetInitialWorkitem(Settings);

    // Async implementation
    mpz_class AsyncStart(Start);
    mpz_class AsyncEnd(AsyncStart);

    // Make the range grow proportionally to the size of numbers being searched.
    unsigned int RangeSize =
        Settings.PrimeSettings.BatchSize
        * Settings.PrimeSettings.ThreadCount
        * mpz_sizeinbase(AsyncStart.get_mpz_t(), 2);

    for (unsigned long long i = 0; (i < Settings.PrimeSettings.BatchCount) & !Quit; i++)
    {
        AsyncEnd += RangeSize;

        std::unique_ptr<mpz_list_list> IoContainer(new mpz_list_list());

        IoContainer->push_back(
            findPrimes(Settings.PrimeSettings.ThreadCount, AsyncStart, AsyncEnd));

        IoPool.EnqueueWorkItem(std::move(IoContainer));

        // Move the start to the end of the range
        AsyncStart = AsyncEnd;
    }
}

void Primebot::RunThreadsClient()
{
    for (unsigned int i = 0; i < Threads.size(); i++)
    {
        Threads[i] = std::move(std::thread(&Primebot::FindPrimesClient, this));
    }
}

void Primebot::RunThreadsStandalone()
{
    // Stand-alone mode, generate starting prime
    mpz_class Start = GetInitialWorkitem(Settings);

    // Make BatchSize grow proportionally to the size of the numbers being searched.
    unsigned int BatchSize = Settings.PrimeSettings.BatchSize * mpz_sizeinbase(Start.get_mpz_t(), 2);

    for (unsigned int i = 0; i < Threads.size(); i++)
    {
        // ((2 * i) + Start)
        mpz_class work(Start);
        work += (2 * i * BatchSize);
        Threads[i] = std::move(std::thread(&Primebot::FindPrimeStandalone, this, std::move(work), i, BatchSize));
    }
}

Primebot::Primebot(const AllPrimebotSettings& Config, NetworkController* NetController) :
    Controller(NetController),
    Settings(Config),
    FileIo(Settings),
    Threads(Config.PrimeSettings.ThreadCount),
    IoPool(1,
        std::bind(&Primebot::ProcessIo, this, std::placeholders::_1)),
    Quit(false),
    Results(new std::vector<mpz_list>(Settings.PrimeSettings.ThreadCount)),
    ResultsCount(0)
{}

Primebot::~Primebot()
{
    Stop();
}

void Primebot::Start()
{
    mpz_class Start;

    if (Settings.NetworkSettings.Client)
    {
        StartClient();
    }
    else
    {
        StartStandalone();
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

        // Drain remaining I/Os
        IoPool.Stop();

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

        // Nothing to wait for, since the Async path runs in a loop that doesn't
        // exit until Quit is set from CTRL+C/server shutdown message.
        Stop();
    }
    else
    {
        std::unique_lock<std::mutex> lock(DoneLock);

        // Wait to exit
        Done.wait(lock);
    }
}

