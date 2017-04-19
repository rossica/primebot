#include <iostream>
#include <string>
#include <memory>

#include "asyncPrimeSearching.h"
#include "networkcontroller.h"
#include "commandparser.h"
#include "prime.h"
#include "pal.h"
#include "fileio.h"

// Global program state
// (Currently used for signal handlers)
AllPrimebotSettings ProgramSettings;
NetworkController* Controller = nullptr;
Primebot* Bot = nullptr;


int main(int argc, char** argv)
{

    /*
    mpz_class a = 3;
    mpz_class b = 5000;
    auto primes = findPrimes(8, a, b);
    for (auto v : primes)
        std::cout << v.get_str() << std::endl; //TODO: strange bug isn't linking operator<< properly.

    int dummy = 0;
    std::cin >> dummy;
    return 0;
    */

    CommandParser Parse(argc, argv);

    ProgramSettings = Parse.ParseArguments();

    if (!ProgramSettings.Run)
    {
        return EXIT_FAILURE;
    }

    if (!RegisterSignalHandler())
    {
        return EXIT_FAILURE;
    }

    if (ProgramSettings.NetworkSettings.Server)
    {
        // Configure for Server
        NetworkController netsrv(ProgramSettings);
        Controller = &netsrv;
        netsrv.Start();
    }
    else if(ProgramSettings.NetworkSettings.Client)
    {
        // Configure for client
        NetworkController netcon(ProgramSettings);
        Primebot pb(ProgramSettings, &netcon);
        Bot = &pb;
        netcon.SetPrimebot(&pb);

        pb.Start();

        pb.WaitForStop();
    }
    else if (ProgramSettings.FileSettings.Flags.Print)
    {
        mpz_list Primes;
        if (ProgramSettings.FileSettings.Flags.Binary)
        {
            Primes = ReadPrimesFromFileBinary(ProgramSettings.FileSettings.Path);
        }
        else
        {
            Primes = ReadPrimesFromFile(ProgramSettings.FileSettings.Path);
        }

        for (mpz_class & p : Primes)
        {
            // Compromise: gmp_printf on windows can't figure out the
            // stdio file handles. But this works fine.
            gmp_fprintf(stdout, "%Zd\n", p.get_mpz_t());
        }
    }
    else
    {
        // Configure standalone
        Primebot pb(ProgramSettings, nullptr);
        Bot = &pb;

        pb.Start();

        // Only way to stop is to CTRL+C, but shutdown cleanly anyway
        pb.WaitForStop();
    }

    return EXIT_SUCCESS;
}
