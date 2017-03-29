#pragma once

#include <thread>
#include <vector>
#include <deque>
#include <atomic>
#include <mutex>
#include <memory>
#include <functional>
#include <condition_variable>

template<class T>
class Threadpool
{
private:
	std::vector<std::thread> Threads;
	std::deque<T> WorkItems;
	std::deque<T> Results;
	std::mutex WorkItemsLock;
	std::mutex ResultsLock;
	std::recursive_mutex WaitLock;
	std::condition_variable_any WaitVariable;
	std::atomic_bool Shutdown;
	int ThreadCount;

	void ThreadFunc();

	void Initialize();

	bool GetResultThread(T& Result);
	bool GetWorkItemThread(T& WorkItem);

	std::function<void(Threadpool<T>&, T)> ProcessWorkItem;
	std::function<void(Threadpool<T>&, T)> ProcessResult;
public:
	Threadpool() = delete;
	Threadpool(const Threadpool&) = delete;
	Threadpool(unsigned int ThreadCount, std::function<void(Threadpool<T>&, T)>&& ProcessWorkitem, std::function<void(Threadpool<T>&, T)>&& ProcessResult);
	~Threadpool();

	void EnqueueWorkItem(T WorkItem);
	void EnqueueResult(T Result);

	void Stop();

	int GetThreadCount() { return ThreadCount; }
};

template<class T>
Threadpool<T>::Threadpool(unsigned int ThreadCount, std::function<void(Threadpool<T>&, T)>&& ProcessWorkitemFunc, std::function<void(Threadpool<T>&, T)>&& ProcessResultFunc) :
    ProcessWorkItem(std::move(ProcessWorkitemFunc)),
    ProcessResult(std::move(ProcessResultFunc)),
    ThreadCount(ThreadCount)
{
	Initialize();
}

template<class T>
Threadpool<T>::~Threadpool()
{
	Stop();
}

template<class T>
void Threadpool<T>::Initialize()
{
	for (int i = 0; i < ThreadCount; i++)
	{
		Threads.push_back(std::thread(std::bind(&Threadpool::ThreadFunc, this)));
	}
	Threads.shrink_to_fit(); // don't waste memory
}

template<class T>
void Threadpool<T>::Stop()
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

	WorkItems.clear();
	Results.clear();
}

template<class T>
bool Threadpool<T>::GetResultThread(T& Result)
{
	std::lock_guard<std::mutex> lock(ResultsLock);

	if (Results.size() == 0)
	{
		return false;
	}

	Result = std::move(Results.front());
	Results.pop_front();

	return true;
}

template<class T>
bool Threadpool<T>::GetWorkItemThread(T& WorkItem)
{
	std::lock_guard<std::mutex> lock(WorkItemsLock);

	if (WorkItems.size() == 0)
	{
		return false;
	}

	WorkItem = std::move(WorkItems.front());
	WorkItems.pop_front();

	return true;
}

template<class T>
void Threadpool<T>::ThreadFunc()
{
	while (true)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(WaitLock);
			// Wait for work to do/wait to exit
			WaitVariable.wait(lock);
			if (Shutdown)
			{
				break;
			}
		}

		// check Results, and process a result
		T Result;
		if (GetResultThread(Result))
		{
			ProcessResult(*this, std::move(Result));
		}

		// check WorkItems, and process a workitem
		T WorkItem;
		if (GetWorkItemThread(WorkItem))
		{
			ProcessWorkItem(*this, std::move(WorkItem));
		}
	}
}

template<class T>
void Threadpool<T>::EnqueueWorkItem(T WorkItem)
{
	{
		std::lock_guard<std::mutex> lock(WorkItemsLock);
		WorkItems.push_back(std::move(WorkItem));
	}
	std::unique_lock<std::recursive_mutex> lock(WaitLock);
	WaitVariable.notify_one();
}

template<class T>
void Threadpool<T>::EnqueueResult(T Result)
{
	{
		std::lock_guard<std::mutex> lock(ResultsLock);
		Results.push_back(std::move(Result));
	}
	std::unique_lock<std::recursive_mutex> lock(WaitLock);
	WaitVariable.notify_one();
}