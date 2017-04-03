#include <iostream>
#include <string>
#include <memory>

#include "prime.h"
//#include "onelockthreadpool.h"
//#include "asyncPrimeSearching.h"
#include "networkcontroller.h"
#include <string.h>

// test code
/*
void ProcessPrime(ThreadContext<std::unique_ptr<int>>& thread, std::unique_ptr<int> workitem)
{
	if (*workitem < 300)
	{
		thread.EnqueueWork(std::make_unique<int>(*workitem + (2 * thread.Pool.GetThreadCount())));
		for (int i = 0; i < 10000000; i++)
		{
			i += *workitem;
			i -= *workitem;
		}
		thread.EnqueueResult(std::move(workitem));
	}
}

void ProcessResult(ThreadContext<std::unique_ptr<int>>& thread, std::unique_ptr<int> result)
{
	std::cout << *result << std::endl;
}

OneLockThreadpool<std::unique_ptr<int>> tp(std::thread::hardware_concurrency(), ProcessPrime, ProcessResult);
*/

int main(int argc, char** argv)
{
	//for (unsigned int i = 1; i <= tp.GetThreadCount(); i++)
	//{
	//	tp.EnqueueWorkItem(std::make_unique<int>((2*i) + 1));
	//}

	//std::this_thread::sleep_for(std::chrono::seconds(20));
	//tp.Stop();
	//std::this_thread::sleep_for(std::chrono::seconds(60));

	/*
	Primebot pb(std::thread::hardware_concurrency(), nullptr);

	pb.Start();

	std::this_thread::sleep_for(std::chrono::seconds(60));
	*/

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


    if (argc > 1 && strcmp(argv[1], "-c") == 0)
    {
        // client code
        AllSettings Settings;
        Settings.NetworkSettings.IPv4.sin_family = AF_INET;
        Settings.NetworkSettings.IPv4.sin_addr = in4addr_loopback;
        Settings.NetworkSettings.IPv4.sin_port = htons(60000);
        Settings.NetworkSettings.Server = false;

        NetworkController netcon(Settings);

        Primebot pb(std::thread::hardware_concurrency(), &netcon);
        netcon.SetPrimebot(&pb);

        pb.Start();

        int dummy = 0;
        std::cout << "Type anything and press enter to exit.";
        std::cin >> dummy;

        pb.Stop();
    }
    else if (argc > 1 && strcmp(argv[1], "-s") == 0)
    {
        // server
        AllSettings Settings;
        Settings.NetworkSettings.IPv4.sin_family = AF_INET;
        Settings.NetworkSettings.IPv4.sin_addr= in4addr_loopback;
        Settings.NetworkSettings.Server = true;

        NetworkController netsrv(Settings);
        netsrv.Start();
    }

    return 0;
}
