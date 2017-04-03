#include <iostream>
#include <string>
#include <memory>

#include "prime.h"
//#include "onelockthreadpool.h"
//#include "asyncPrimeSearching.h"
#include "networkcontroller.h"
#include "commandparser.h"


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

    AllPrimebotSettings Settings;
    CommandParser Parse(argc, argv);

    Settings = Parse.ParseArguments();

    if (!Settings.Run)
    {
        return 0;
    }

    if (Settings.NetworkSettings.Server)
    {
        // Configure for Server
        NetworkController netsrv(Settings);
        netsrv.Start();
    }
    else if(Settings.NetworkSettings.Client)
    {
        // Configure for client
        NetworkController netcon(Settings);
        Primebot pb(Settings, &netcon);
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
        Primebot pb(Settings, nullptr);

        pb.Start();

        int dummy = 0;
        std::cout << "Type anything and press enter to exit.";
        std::cin >> dummy;

        pb.Stop();
    }

    return 0;
}
