#include <iostream>
//#include "threadpool.h"
#include "locklessthreadpool.h"

void ProcessPrime(ThreadContext<std::unique_ptr<int>, decltype(ProcessPrime), decltype(ProcessResult)>* thread, std::unique_ptr<int> workitem);
void ProcessResult(ThreadContext<std::unique_ptr<int>, decltype(ProcessPrime), decltype(ProcessResult)>* thread, std::unique_ptr<int> result);

LocklessThreadpool<std::unique_ptr<int>, decltype(ProcessPrime), decltype(ProcessResult)> tp(std::thread::hardware_concurrency());

void ProcessPrime(ThreadContext<std::unique_ptr<int>, decltype(ProcessPrime), decltype(ProcessResult)>* thread, std::unique_ptr<int> workitem)
{
	if (*workitem < 300)
	{
		thread->EnqueueWork(std::make_unique<int>(*workitem + (2 * tp.GetThreadCount())));
		for (int i = 0; i < 1000000; i++)
		{
			i += *workitem;
			i -= *workitem;
		}
		thread->EnqueueResult(std::move(workitem));
	}
}

void ProcessResult(ThreadContext<std::unique_ptr<int>, decltype(ProcessPrime), decltype(ProcessResult)>* thread, std::unique_ptr<int> result)
{
	std::cout << *result << std::endl;
}


int main(char* argv, int argc)
{
	for (int i = 0, j=1; i < tp.GetThreadCount(); i++, j+=2)
	{
		tp.EnqueueWorkItem(std::make_unique<int>(j));
	}

	std::this_thread::sleep_for(std::chrono::seconds(20));
	tp.Stop();
	std::this_thread::sleep_for(std::chrono::seconds(60));

	return 0;
}