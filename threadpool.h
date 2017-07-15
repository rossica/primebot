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

    std::pair<bool,T> GetWorkItemThread();

	std::function<void(T&&)> ProcessWorkItem;
public:
	Threadpool() = delete;
	Threadpool(const Threadpool&) = delete;
	Threadpool(unsigned int ThreadCount, std::function<void(T&&)>&& ProcessWorkitem);
	~Threadpool();

    void EnqueueWorkItem(T&& WorkItem);

	void Stop();

    unsigned int GetThreadCount() { return ThreadCount; }
    auto GetWorkItemCount() { std::lock_guard<std::mutex> lock(WorkItemsLock); return WorkItems.size(); }
};

template<class T,class C>
Threadpool<T,C>::Threadpool(unsigned int ThreadCount, std::function<void(T&&)>&& ProcessWorkitemFunc) :
    Threads(ThreadCount),
    Stopping(false),
    Stopped(false),
    ThreadCount(ThreadCount),
    ProcessWorkItem(std::move(ProcessWorkitemFunc))
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

    Stopped = true;

    {
        std::unique_lock<std::recursive_mutex> lock(WaitLock);
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
inline std::pair<bool,T> Threadpool<T,C>::GetWorkItemThread()
{
	std::lock_guard<std::mutex> lock(WorkItemsLock);

	if (WorkItems.size() == 0)
	{
        return std::make_pair<bool, T>(false, {});
	}

	std::pair<bool,T> Workitem(true, std::move(WorkItems.front()));
	WorkItems.pop_front();

	return Workitem;
}

template<class T,class C>
inline void Threadpool<T,C>::ThreadFunc()
{
	while (!Stopped)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(WaitLock);
			// Wait for work to do/wait to exit
            WaitVariable.wait(lock, [this] () -> bool { return (WorkItems.size() > 0) || Stopped; });
		}

        // check WorkItems, and process a workitem
        std::pair<bool, T> Workitem = GetWorkItemThread();
        if (Workitem.first)
        {
            ProcessWorkItem(std::move(Workitem.second));
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