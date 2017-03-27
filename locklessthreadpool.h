#pragma once

#include <thread>
#include <vector>
#include <deque>
#include <mutex>

template<class Work_T, typename WorkCallable, typename ResultCallable>
class ThreadContext {
private:
	std::thread Thread;
	std::deque<Work_T> WorkItems;
	std::deque<Work_T> Results;
	std::recursive_mutex Sync;
	bool Shutdown;
	void ThreadFunc();
	WorkCallable ProcessWorkitem;
	ResultCallable ProcessResult;
public:
	void EnqueueWork(Work_T Work);
	void EnqueueResult(Work_T Result);
	void Stop();
	ThreadContext(WorkCallable && ProcessWorkitem, ResultCallable && ProcessResult);
};

template<class Work_T, typename WorkCallable, typename ResultCallable>
class LocklessThreadpool
{
private:
	std::vector<ThreadContext<Work_T, WorkCallable, ResultCallable>> Threads;
	unsigned int CurrentThread;
	unsigned int ThreadCount;

	void Initialize();

public:
	LocklessThreadpool() = delete;
	LocklessThreadpool(const LocklessThreadpool&) = delete;
	LocklessThreadpool(unsigned int ThreadCount);
	~LocklessThreadpool();

	void EnqueueWorkItem(Work_T WorkItem);

	void Stop();

	unsigned int GetThreadCount() { return ThreadCount; }
};

template<class Work_T, typename WorkCallable, typename ResultCallable>
inline void ThreadContext<Work_T, WorkCallable, ResultCallable>::EnqueueWork(Work_T Work)
{
	std::lock_guard<std::recursive_mutex> lock(Sync);
	WorkItems.push_back(Work);
}

template<class Work_T, typename WorkCallable, typename ResultCallable>
inline void ThreadContext<Work_T, WorkCallable, ResultCallable>::EnqueueResult(Work_T Result)
{
	std::lock_guard<std::recursive_mutex> lock(Sync);
	Results.push_back(Result);
}

template<class Work_T, typename WorkCallable, typename ResultCallable>
inline void ThreadContext<Work_T, WorkCallable, ResultCallable>::Stop()
{
	if (Thread.joinable())
	{
		Shutdown = true;
		Thread.join();
		Results.clear();
		WorkItems.clear();
	}
}

template<class Work_T, typename WorkCallable, typename ResultCallable>
inline ThreadContext<Work_T, WorkCallable, ResultCallable>::ThreadContext(WorkCallable && ProcessWorkitem, ResultCallable && ProcessResult)
{
	this->ProcessWorkitem = ProcessWorkitem;
	this->ProcessResult = ProcessResult;

	this->Thread = std::thread(&ThreadContext<Work_T, WorkCallable, ResultCallable>::ThreadFunc, this);
}

template<class Work_T, typename WorkCallable, typename ResultCallable>
inline void ThreadContext<Work_T, WorkCallable, ResultCallable>::ThreadFunc()
{
	bool WasWorkPerformed;
	while (!Shutdown)
	{
		WasWorkPerformed = false;
		{
			std::lock_guard<std::recursive_mutex> lock(Sync);

			if (WorkItems.size() > 0)
			{
				Work_T value = std::move(WorkItems.front());
				WorkItems.pop_front();
				ProcessWorkitem(this, value);
				WasWorkPerformed = true;
			}

			if (Results.size() > 0)
			{
				Work_T value = std::move(Results.front());
				Results.pop_front();
				ProcessResult(this, value);
				WasWorkPerformed = true;
			}
		}

		if (!WasWorkPerformed)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
}

template<class Work_T, typename WorkCallable, typename ResultCallable>
inline void LocklessThreadpool<Work_T, WorkCallable, ResultCallable>::Initialize()
{
	for (int i = 0; i < ThreadCount; i++)
	{
		Threads.push_back(ThreadContext<Work_T, WorkCallable, ResultCallable>(WorkCallable, ResultCallable));
	}
	Threads.shrink_to_fit();
}

template<class Work_T, typename WorkCallable, typename ResultCallable>
inline LocklessThreadpool<Work_T, WorkCallable, ResultCallable>::LocklessThreadpool(unsigned int ThreadCount)
{
	this->ThreadCount = ThreadCount;
	CurrentThread = 0;

	Threads.reserve(ThreadCount);

	Initialize();
}

template<class Work_T, typename WorkCallable, typename ResultCallable>
inline LocklessThreadpool<Work_T, WorkCallable, ResultCallable>::~LocklessThreadpool()
{
	Stop();
}

template<class Work_T, typename WorkCallable, typename ResultCallable>
inline void LocklessThreadpool<Work_T, WorkCallable, ResultCallable>::EnqueueWorkItem(Work_T WorkItem)
{
	Threads[CurrentThread].EnqueueWork(WorkItem);
	CurrentThread = (CurrentThread + 1) % ThreadCount;
}

template<class Work_T, typename WorkCallable, typename ResultCallable>
inline void LocklessThreadpool<Work_T, WorkCallable, ResultCallable>::Stop()
{
	for (int i = 0; i < ThreadCount; i++)
	{
		Threads[i].Stop();
	}
}
