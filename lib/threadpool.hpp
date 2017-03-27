#include <chrono>

template<typename Number>
Threadpool<Number>::Threadpool(unsigned int ThreadCount, std::function<void(Threadpool<Number>&, Number)> ProcessWorkitemFunc, std::function<void(Threadpool<Number>&, Number)> ProcessResultFunc)
{
	ProcessWorkItem = ProcessWorkitemFunc;
	ProcessResult = ProcessResultFunc;
	this->ThreadCount = ThreadCount;

	Initialize();
}

template<typename Number>
Threadpool<Number>::~Threadpool()
{
	Stop();
}

template<typename Number>
void Threadpool<Number>::Initialize()
{
	if (ThreadCount == 0)
	{
		ThreadCount = 1;
	}

	for (int i = 0; i < ThreadCount; i++)
	{
		Threads.push_back(std::thread(std::bind(&Threadpool<Number>::ThreadFunc, this)));
	}
	Threads.shrink_to_fit(); // don't waste memory
}

template<typename Number>
void Threadpool<Number>::Stop()
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

template<typename Number>
std::unique_ptr<Number> Threadpool<Number>::GetResultThread()
{
	std::lock_guard<std::mutex> lock(ResultsLock);

	if (Results.size() == 0)
	{
		return nullptr;
	}

	std::unique_ptr<Number> Value = std::make_unique<Number>(std::move(Results.front()));
	Results.pop_front();

	return Value;
}

template<typename Number>
std::unique_ptr<Number> Threadpool<Number>::GetWorkItemThread()
{
	std::lock_guard<std::mutex> lock(WorkItemsLock);

	if (WorkItems.size() == 0)
	{
		return nullptr;
	}

	std::unique_ptr<Number> Value = std::make_unique<Number>(std::move(WorkItems.front()));
	WorkItems.pop_front();

	return Value;
}

template<typename Number>
void Threadpool<Number>::ThreadFunc()
{
	std::unique_lock<std::recursive_mutex> lock(WaitLock);
	while (!Shutdown)
	{
		// Wait for work to do
		WaitVariable.wait(lock);

		// check Results, and process a result
		std::unique_ptr<Number> Result = GetResultThread();
		if (Result != nullptr) {
			ProcessResult(*this, std::move(*Result));
		}


		// check WorkItems, and process a workitem 
		std::unique_ptr<Number> WorkItem = GetWorkItemThread();
		if (WorkItem != nullptr) {
			ProcessWorkItem(*this, std::move(*WorkItem));
		}
	}
}

template<typename Number>
void Threadpool<Number>::EnqueueWorkItem(Number WorkItem)
{
	{
		std::lock_guard<std::mutex> lock(WorkItemsLock);
		WorkItems.push_back(std::move(WorkItem));
	}
	std::unique_lock<std::recursive_mutex> lock(WaitLock);
	WaitVariable.notify_one();
}

template<typename Number>
void Threadpool<Number>::EnqueueResult(Number Result)
{
	{
		std::lock_guard<std::mutex> lock(ResultsLock);
		Results.push_back(std::move(Result));
	}
	std::unique_lock<std::recursive_mutex> lock(WaitLock);
	WaitVariable.notify_one();
}
