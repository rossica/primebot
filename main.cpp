#include <iostream>
#include "threadpool.h"


void ProcessPrime(Threadpool& pool, std::unique_ptr<void*> workitem)
{
	if (*((int*)*workitem) < 200)
	{
		int* t = new int;
		*t = *((int*)*workitem) + (2 * pool.GetThreadCount());
		pool.EnqueueWorkItem(std::move(std::make_unique<void*>(t)));
		pool.EnqueueResult(std::move(workitem));
	}
}

void ProcessResult(Threadpool& pool, std::unique_ptr<void*> result)
{
	std::cout << *(int*)*result << std::endl;
}

Threadpool tp(std::thread::hardware_concurrency(), ProcessPrime, ProcessResult);


int main(char* argv, int argc)
{
	for (int i = 0, j=1; i < tp.GetThreadCount(); i++, j+=2)
	{
		int* t = new int;
		*t = j;
		tp.EnqueueWorkItem(std::move(std::make_unique<void*>(t)));
	}

	std::this_thread::sleep_for(std::chrono::seconds(10));
	tp.Stop();
	std::this_thread::sleep_for(std::chrono::seconds(60));

	return 0;
}