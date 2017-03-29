#pragma once

#include <thread>
#include <vector>
#include <deque>
#include <mutex>

template<class Work_T>
class OneLockThreadpool;

template<class Work_T>
class ThreadContext {
private:
    std::thread Thread;
    std::deque<Work_T> WorkItems;
    std::deque<Work_T> Results;
    std::recursive_mutex Sync;
    bool Shutdown;
    void ThreadFunc();
    std::function<void(ThreadContext<Work_T>&, Work_T)> ProcessWorkitem;
    std::function<void(ThreadContext<Work_T>&, Work_T)> ProcessResult;
public:
    const OneLockThreadpool<Work_T>& Pool;
    void EnqueueWork(Work_T Work);
    void EnqueueResult(Work_T Result);
    void Stop();
    ThreadContext() = delete;
    ThreadContext(OneLockThreadpool<Work_T>& Parent, std::function<void(ThreadContext<Work_T>&, Work_T)> ProcessWorkitem, std::function<void(ThreadContext<Work_T>&, Work_T)> ProcessResult);
    ThreadContext(ThreadContext<Work_T>&) = delete;
    ThreadContext(ThreadContext<Work_T>&&);
    ~ThreadContext();
};

template<class Work_T>
class OneLockThreadpool
{
private:
    std::vector<ThreadContext<Work_T>> Threads;
    unsigned int CurrentThread;
    unsigned int ThreadCount;

    void Initialize(std::function<void(ThreadContext<Work_T>&, Work_T)> ProcessWorkitem, std::function<void(ThreadContext<Work_T>&, Work_T)> ProcessResult);

public:
    OneLockThreadpool() = delete;
    OneLockThreadpool(const OneLockThreadpool&) = delete;
    OneLockThreadpool(unsigned int ThreadCount, std::function<void(ThreadContext<Work_T>&, Work_T)> WorkProc, std::function<void(ThreadContext<Work_T>&, Work_T)> ResultProc);
    ~OneLockThreadpool();

    void EnqueueWorkItem(Work_T WorkItem);

    void Stop();

    unsigned int GetThreadCount() const { return ThreadCount; }
};

template<class Work_T>
inline void ThreadContext<Work_T>::EnqueueWork(Work_T Work)
{
    std::lock_guard<std::recursive_mutex> lock(Sync);
    WorkItems.push_back(std::move(Work));
}

template<class Work_T>
inline void ThreadContext<Work_T>::EnqueueResult(Work_T Result)
{
    std::lock_guard<std::recursive_mutex> lock(Sync);
    Results.push_back(std::move(Result));
}

template<class Work_T>
inline void ThreadContext<Work_T>::Stop()
{
    if (Thread.joinable())
    {
        Shutdown = true;
        Thread.join();
        Results.clear();
        WorkItems.clear();
    }
}

template<class Work_T>
inline void ThreadContext<Work_T>::ThreadFunc()
{
    while (!Shutdown)
    {
        bool WasWorkPerformed = false;
        {
            std::lock_guard<std::recursive_mutex> lock(Sync);

            if (WorkItems.size() > 0)
            {
                Work_T value(std::move(WorkItems.front()));
                WorkItems.pop_front();
                ProcessWorkitem(*this, std::move(value));
                WasWorkPerformed = true;
            }

            if (Results.size() > 0)
            {
                Work_T value(std::move(Results.front()));
                Results.pop_front();
                ProcessResult(*this, std::move(value));
                WasWorkPerformed = true;
            }
        }

        if (!WasWorkPerformed)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }
}

template<class Work_T>
inline ThreadContext<Work_T>::ThreadContext(OneLockThreadpool<Work_T>& Parent, std::function<void(ThreadContext<Work_T>&, Work_T)> ProcessWorkitem, std::function<void(ThreadContext<Work_T>&, Work_T)> ProcessResult) :
    Shutdown(false),
    ProcessWorkitem(ProcessWorkitem),
    ProcessResult(ProcessResult),
    Pool(Parent)
{
    // This must be initialized after Shutdown has been initialized
    Thread = std::thread(std::bind(&ThreadContext::ThreadFunc, this));
}

// I don't trust myself to have written this correctly
template<class Work_T>
inline ThreadContext<Work_T>::ThreadContext(ThreadContext<Work_T>&& Other) :
    Pool(Other.Pool)
{
    // Danger: if another thread is actively altering this object before
    // this constructor is finished, the swap() calls below might lose
    // workitems/results.
    std::lock(Sync, Other.Sync);

    std::swap(ProcessWorkitem, Other.ProcessWorkitem);
    std::swap(ProcessResult, Other.ProcessResult);

    // why copy the other thread's shutdown?
    Shutdown = Other.Shutdown;
    WorkItems.swap(Other.WorkItems);
    Results.swap(Other.Results);

    Other.Shutdown = true;
    Other.Sync.unlock();
    Sync.unlock();
}

template<class Work_T>
inline ThreadContext<Work_T>::~ThreadContext()
{
    Stop();
}

template<class Work_T>
inline void OneLockThreadpool<Work_T>::Initialize(std::function<void(ThreadContext<Work_T>&, Work_T)> ProcessWorkitem, std::function<void(ThreadContext<Work_T>&, Work_T)> ProcessResult)
{
    for (unsigned int i = 0; i < ThreadCount; i++)
    {
        Threads.emplace_back(*this, ProcessWorkitem, ProcessResult);
    }
    Threads.shrink_to_fit();
}

template<class Work_T>
inline OneLockThreadpool<Work_T>::OneLockThreadpool(unsigned int ThreadCount, std::function<void(ThreadContext<Work_T>&, Work_T)> WorkProc, std::function<void(ThreadContext<Work_T>&, Work_T)> ResultProc)
{
    this->ThreadCount = ThreadCount;
    CurrentThread = 0;

    Threads.reserve(ThreadCount);

    Initialize(WorkProc, ResultProc);
}

template<class Work_T>
inline OneLockThreadpool<Work_T>::~OneLockThreadpool()
{
    Stop();
}

template<class Work_T>
inline void OneLockThreadpool<Work_T>::EnqueueWorkItem(Work_T WorkItem)
{
    Threads[CurrentThread].EnqueueWork(std::move(WorkItem));
    CurrentThread = (CurrentThread + 1) % ThreadCount;
}

template<class Work_T>
inline void OneLockThreadpool<Work_T>::Stop()
{
    for (unsigned int i = 0; i < ThreadCount; i++)
    {
        Threads[i].Stop();
    }
}
