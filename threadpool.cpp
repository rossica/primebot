#include "threadpool.h"

#include <chrono>

Threadpool::Threadpool(std::function<void(std::unique_ptr<void*>)>& ProcessWorkitemFunc, std::function<void(std::unique_ptr<void*>)>& ProcessResultFunc)
{
	ProcessWorkItem = ProcessWorkitemFunc;
	ProcessResult = ProcessResultFunc;

	Initialize();
}

Threadpool::~Threadpool()
{
	Shutdown = true;

	for (int i = 0; i < Threads.size(); i++)
	{
		Threads[i].join();
	}
}

void Threadpool::Initialize()
{
	int ThreadCount = std::thread::hardware_concurrency();

	if (ThreadCount == 0)
	{
		ThreadCount = 1;
	}

	for (int i = 0; i < ThreadCount; i++)
	{
		Threads.push_back(std::thread(std::bind(&Threadpool::ThreadFunc, this)));
	}
	Threads.shrink_to_fit(); // don't waste memory
}

std::unique_ptr<void*> Threadpool::GetResultThread()
{
	if (!ResultsLock.try_lock())
	{
		return nullptr;
	}

	std::unique_ptr<void*> Value = std::move(Results.front());
	Results.pop_front();

	ResultsLock.unlock();
	return Value;
}

std::unique_ptr<void*> Threadpool::GetWorkItemThread()
{
	if (!WorkItemsLock.try_lock())
	{
		return nullptr;
	}

	std::unique_ptr<void*> Value = std::move(WorkItems.front());
	WorkItems.pop_front();

	WorkItemsLock.unlock();
	return Value;
}

void Threadpool::ThreadFunc()
{
	while (!Shutdown)
	{
		// check Results, and process a result
		std::unique_ptr<void*> Result = GetResultThread();
		if (Result != nullptr)
		{
			ProcessResult(std::move(Result));
		}

		// check WorkItems, and process a workitem
		std::unique_ptr<void*> WorkItem = GetWorkItemThread();
		if (WorkItem != nullptr)
		{
			ProcessWorkItem(std::move(WorkItem));
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}


void Threadpool::EnqueueWorkItem(std::unique_ptr<void*> WorkItem)
{
	std::lock_guard<std::mutex> lock(WorkItemsLock);
	WorkItems.push_back(std::move(WorkItem));
}

void Threadpool::EnqueueResult(std::unique_ptr<void*> Result)
{
	std::lock_guard<std::mutex> lock(ResultsLock);
	Results.push_back(std::move(Result));
}