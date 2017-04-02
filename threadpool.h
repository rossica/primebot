#pragma once

#include <thread>
#include <vector>
#include <deque>
#include <atomic>
#include <mutex>
#include <memory>
#include <functional>
#include <condition_variable>

template<class T,class C = std::deque<T>>
class Threadpool
{
private:
	std::vector<std::thread> Threads;
    C WorkItems;
    C Results;
	std::mutex WorkItemsLock;
	std::mutex ResultsLock;
	std::recursive_mutex WaitLock;
	std::condition_variable_any WaitVariable;
	std::atomic_bool Stopping;
    std::atomic_bool Stopped;
	int ThreadCount;

	void ThreadFunc();

	void Initialize();

    bool GetResultThread(T&& Result);
    bool GetWorkItemThread(T&& WorkItem);

	std::function<void(Threadpool<T,C>&, T)> ProcessWorkItem;
	std::function<void(Threadpool<T,C>&, T)> ProcessResult;
public:
	Threadpool() = delete;
	Threadpool(const Threadpool&) = delete;
	Threadpool(unsigned int ThreadCount, std::function<void(Threadpool<T,C>&, T)>&& ProcessWorkitem, std::function<void(Threadpool<T,C>&, T)>&& ProcessResult);
	~Threadpool();

    void EnqueueWorkItem(T&& WorkItem);
    void EnqueueResult(T&& Result);

	void Stop();

	int GetThreadCount() { return ThreadCount; }
};

template<class T,class C>
Threadpool<T,C>::Threadpool(unsigned int ThreadCount, std::function<void(Threadpool<T,C>&, T)>&& ProcessWorkitemFunc, std::function<void(Threadpool<T,C>&, T)>&& ProcessResultFunc) :
    ProcessWorkItem(std::move(ProcessWorkitemFunc)),
    ProcessResult(std::move(ProcessResultFunc)),
    ThreadCount(ThreadCount)
{
	Initialize();
}

template<class T,class C>
Threadpool<T,C>::~Threadpool()
{
	Stop();
}

template<class T,class C>
inline void Threadpool<T,C>::Initialize()
{
	for (int i = 0; i < ThreadCount; i++)
	{
		Threads.push_back(std::thread(std::bind(&Threadpool::ThreadFunc, this)));
	}
	Threads.shrink_to_fit(); // don't waste memory
}

template<class T,class C>
inline void Threadpool<T,C>::Stop()
{
    Stopping = true;

    // Drain any remaining results
    ResultsLock.lock();
    while (Results.size() > 0)
    {
        ResultsLock.unlock();
        {
            std::unique_lock<std::recursive_mutex> lock(WaitLock);
            WaitVariable.notify_all();
        }
        ResultsLock.lock();
    }
    ResultsLock.unlock();

    {
        std::unique_lock<std::recursive_mutex> lock(WaitLock);
        Stopped = true;
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
    Threads.clear();
}

template<class T,class C>
inline bool Threadpool<T,C>::GetResultThread(T&& Result)
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

template<class T,class C>
inline bool Threadpool<T,C>::GetWorkItemThread(T&& WorkItem)
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

template<class T,class C>
inline void Threadpool<T,C>::ThreadFunc()
{
	while (!Stopped)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(WaitLock);
			// Wait for work to do/wait to exit
            WaitVariable.wait_for(lock, std::chrono::seconds(2));
		}

        // check WorkItems, and process a workitem
        T WorkItem{};
        if (GetWorkItemThread(std::move(WorkItem)))
        {
            ProcessWorkItem(*this, std::move(WorkItem));
        }

        // check Results, and process a result
        T Result{};
        if (GetResultThread(std::move(Result)))
        {
            ProcessResult(*this, std::move(Result));
        }
	}
}

template<class T,class C>
inline void Threadpool<T,C>::EnqueueWorkItem(T&& WorkItem)
{
    if (Stopping)
    {
        return;
    }
	{
		std::lock_guard<std::mutex> lock(WorkItemsLock);
		WorkItems.push_back(std::move(WorkItem));
	}
    {
        std::unique_lock<std::recursive_mutex> lock(WaitLock);
        WaitVariable.notify_all();
    }
}

template<class T,class C>
inline void Threadpool<T,C>::EnqueueResult(T&& Result)
{
	{
		std::lock_guard<std::mutex> lock(ResultsLock);
		Results.push_back(std::move(Result));
	}
    {
        std::unique_lock<std::recursive_mutex> lock(WaitLock);
        WaitVariable.notify_all();
    }
}