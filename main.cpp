#include <iostream>
#include <string>
#include <memory>

#include "prime.h"
//#include "onelockthreadpool.h"
#include "asyncPrimeSearching.h"
#include "networkcontroller.h"
#include "commandparser.h"
#include "pal.h"

// Global program state
// (Currently used for signal handlers
AllPrimebotSettings ProgramSettings;
NetworkController* Controller = nullptr;
Primebot* Bot = nullptr;


int main(int argc, char** argv)
{

    /*
    mpz_class a = 3;
    mpz_class b = 5000;
    auto primes = findPrimes(a, b);
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

        int dummy = 0;
        std::cout << "Type anything and press enter to exit.";
        std::cin >> dummy;

        pb.Stop();
    }
    else
    {
        // Configure standalone
        Primebot pb(ProgramSettings, nullptr);
        Bot = &pb;

        pb.Start();

        int dummy = 0;
        std::cout << "Type anything and press enter to exit.";
        std::cin >> dummy;

        pb.Stop();
    }

    return EXIT_SUCCESS;
}
