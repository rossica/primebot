#include "threadpool.h"

#include <chrono>

Threadpool::Threadpool(unsigned int ThreadCount, std::function<void(Threadpool&, std::unique_ptr<void*>)> ProcessWorkitemFunc, std::function<void(Threadpool&, std::unique_ptr<void*>)> ProcessResultFunc)
{
	ProcessWorkItem = ProcessWorkitemFunc;
	ProcessResult = ProcessResultFunc;
	this->ThreadCount = ThreadCount;

	Initialize();
}

Threadpool::~Threadpool()
{
	Stop();
}

void Threadpool::Initialize()
{
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

void Threadpool::Stop()
{
	{
		std::unique_lock<std::recursive_mutex> lock(WaitLock);
		Shutdown = true;
		WaitVariable.notify_all();
	}

	for (int i = 0; i < Threads.size(); i++)
	{
		if (Threads[i].joinable())
		{
			Threads[i].join();
		}
	}
}

std::unique_ptr<void*> Threadpool::GetResultThread()
{
	std::lock_guard<std::mutex> lock(ResultsLock);

	if (Results.size() == 0)
	{
		return nullptr;
	}

	std::unique_ptr<void*> Value = std::move(Results.front());
	Results.pop_front();

	return Value;
}

std::unique_ptr<void*> Threadpool::GetWorkItemThread()
{
	std::lock_guard<std::mutex> lock(WorkItemsLock);

	if (WorkItems.size() == 0)
	{
		return nullptr;
	}

	std::unique_ptr<void*> Value = std::move(WorkItems.front());
	WorkItems.pop_front();

	return Value;
}

void Threadpool::ThreadFunc()
{
	std::unique_lock<std::recursive_mutex> lock(WaitLock);
	while (!Shutdown)
	{
		// Wait for work to do
		WaitVariable.wait(lock);

		// check Results, and process a result
		std::unique_ptr<void*> Result = GetResultThread();
		if (Result != nullptr)
		{
			ProcessResult(*this, std::move(Result));
		}

		// check WorkItems, and process a workitem
		std::unique_ptr<void*> WorkItem = GetWorkItemThread();
		if (WorkItem != nullptr)
		{
			ProcessWorkItem(*this, std::move(WorkItem));
		}
	}
}


void Threadpool::EnqueueWorkItem(std::unique_ptr<void*> WorkItem)
{
	{
		std::lock_guard<std::mutex> lock(WorkItemsLock);
		WorkItems.push_back(std::move(WorkItem));
	}
	std::unique_lock<std::recursive_mutex> lock(WaitLock);
	WaitVariable.notify_one();
}

void Threadpool::EnqueueResult(std::unique_ptr<void*> Result)
{
	{
		std::lock_guard<std::mutex> lock(ResultsLock);
		Results.push_back(std::move(Result));
	}
	std::unique_lock<std::recursive_mutex> lock(WaitLock);
	WaitVariable.notify_one();
}