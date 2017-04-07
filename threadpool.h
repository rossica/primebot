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
	std::mutex WorkItemsLock;
	std::recursive_mutex WaitLock;
	std::condition_variable_any WaitVariable;
	std::atomic_bool Stopping;
    std::atomic_bool Stopped;
    unsigned int ThreadCount;

	void ThreadFunc();

	void Initialize();

    bool GetWorkItemThread(T&& WorkItem);

	std::function<void(Threadpool<T,C>&, T)> ProcessWorkItem;
public:
	Threadpool() = delete;
	Threadpool(const Threadpool&) = delete;
	Threadpool(unsigned int ThreadCount, std::function<void(Threadpool<T,C>&, T)>&& ProcessWorkitem);
	~Threadpool();

    void EnqueueWorkItem(T&& WorkItem);

	void Stop();

    unsigned int GetThreadCount() { return ThreadCount; }
};

template<class T,class C>
Threadpool<T,C>::Threadpool(unsigned int ThreadCount, std::function<void(Threadpool<T,C>&, T)>&& ProcessWorkitemFunc) :
    ThreadCount(ThreadCount),
    ProcessWorkItem(std::move(ProcessWorkitemFunc)),
    Threads(ThreadCount)
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
    for (unsigned i = 0; i < ThreadCount; i++)
    {
        Threads[i] = std::move(std::thread(&Threadpool::ThreadFunc, this));
    }
}

template<class T,class C>
inline void Threadpool<T,C>::Stop()
{
    if (Stopped || Stopping)
    {
        return;
    }

    Stopping = true;

    // Drain any remaining workitems
    WorkItemsLock.lock();
    while (WorkItems.size() > 0)
    {
        WorkItemsLock.unlock();
        {
            std::unique_lock<std::recursive_mutex> lock(WaitLock);
            WaitVariable.notify_all();
        }
        WorkItemsLock.lock();
    }
    WorkItemsLock.unlock();

    {
        std::unique_lock<std::recursive_mutex> lock(WaitLock);
        Stopped = true;
        WaitVariable.notify_all();
    }

	for (unsigned i = 0; i < Threads.size(); i++)
	{
		if (Threads[i].joinable())
		{
			Threads[i].join();
		}
	}

    WorkItems.clear();
    Threads.clear();
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