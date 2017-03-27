#pragma once

#include <thread>
#include <vector>
#include <deque>
#include <atomic>
#include <mutex>
#include <memory>
#include <functional>
#include <condition_variable>

template<typename Number>
class Threadpool
{
private:
	std::vector<std::thread> Threads;
	std::deque<Number> WorkItems;
	std::deque<Number> Results;
	std::mutex WorkItemsLock;
	std::mutex ResultsLock;
	std::recursive_mutex WaitLock;
	std::condition_variable_any WaitVariable;
	std::atomic_bool Shutdown;
	int ThreadCount;

	void ThreadFunc();

	void Initialize();

	std::unique_ptr<Number> GetResultThread();
	std::unique_ptr<Number> GetWorkItemThread();

	std::function<void(Threadpool<Number>&, Number)> ProcessWorkItem;
	std::function<void(Threadpool<Number>&, Number)> ProcessResult;
public:
	Threadpool() = delete;
	Threadpool(const Threadpool&) = delete;
	Threadpool(unsigned int ThreadCount, std::function<void(Threadpool<Number>&, Number)> ProcessWorkitem, std::function<void(Threadpool<Number>&, Number)> ProcessResult);
	~Threadpool();

	void EnqueueWorkItem(Number WorkItem);
	void EnqueueResult(Number Result);

	void Stop();

	int GetThreadCount() { return ThreadCount; }
};


#include "threadpool.hpp"