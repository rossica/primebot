#pragma once

#include <thread>
#include <vector>
#include <deque>
#include <atomic>
#include <mutex>
#include <memory>
#include <functional>

class Threadpool
{
private:
	std::vector<std::thread> Threads;
	std::deque<std::unique_ptr<void*>> WorkItems;
	std::deque<std::unique_ptr<void*>> Results;
	std::mutex WorkItemsLock;
	std::mutex ResultsLock;
	std::atomic_bool Shutdown;

	void ThreadFunc();

	void Initialize();

	std::unique_ptr<void*> GetResultThread();
	std::unique_ptr<void*> GetWorkItemThread();

	std::function<void(std::unique_ptr<void*>)> ProcessWorkItem;
	std::function<void(std::unique_ptr<void*>)> ProcessResult;
public:
	Threadpool() = delete;
	Threadpool(const Threadpool&) = delete;
	Threadpool(std::function<void(std::unique_ptr<void*>)>& ProcessWorkitem, std::function<void(std::unique_ptr<void*>)>& ProcessResult);
	~Threadpool();

	void EnqueueWorkItem(std::unique_ptr<void*> WorkItem);
	void EnqueueResult(std::unique_ptr<void*> Result);
};