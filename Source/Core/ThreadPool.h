// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace core
{
	class FunctionWrapper
	{
	public:
		template<typename Function>
		FunctionWrapper(Function&& function) : m_impl(new ImplType<Function>(std::move(function))) {}
	
		FunctionWrapper(FunctionWrapper&& other) noexcept : m_impl(std::move(other.m_impl)) {}

		FunctionWrapper& operator=(FunctionWrapper&& other) noexcept;

		void operator()() { m_impl->Call(); }


	private:
		struct ImplBase
		{
			virtual ~ImplBase() {}
			virtual inline void Call() = 0;
		};
		std::unique_ptr<ImplBase> m_impl;

		template<typename Function>
		struct ImplType : ImplBase
		{
			ImplType(Function&& function) : m_function(std::move(function)) {}

			inline void Call() override { m_function(); }

		private:
			Function m_function;
		};

	private:
		FunctionWrapper() = delete;
		FunctionWrapper(const FunctionWrapper&) = delete;
		FunctionWrapper(FunctionWrapper&) = delete;
		FunctionWrapper& operator=(const FunctionWrapper&) = delete;
	};


	class ThreadPool
	{
	public:
		static ThreadPool* Get(); // Singleton functionality


	public:
		ThreadPool();

		ThreadPool(ThreadPool&&) noexcept = default;
		ThreadPool& operator=(ThreadPool&&) noexcept = default;
		~ThreadPool() = default;

		void Startup();
		void Stop();

		template<typename FunctionType>
		std::future<typename std::invoke_result<FunctionType>::type> EnqueueJob(FunctionType job); // Producer

		static void NameCurrentThread(wchar_t const* threadName);

	private:
		void ExecuteJobs(); // Consumer loop

		void GrowThreadPool(size_t currentJobQueueSize);
		bool ShrinkThreadPool(size_t currentJobQueueSize, std::thread::id currentThread); // Returns true if calling thread should terminate
		void AddWorkerThread();


	private:
		size_t m_minThreadCount;
		static constexpr size_t k_targetJobsPerThread = 4;
		bool m_isRunning;

		std::mutex m_jobQueueMutex;
		std::condition_variable m_jobQueueCV;

		std::queue<FunctionWrapper> m_jobQueue;

		std::unordered_map<std::thread::id, std::thread> m_workerThreads;
		std::list<std::thread> m_workerThreadsToJoin;
		std::shared_mutex m_workerThreadsMutex;


	private: // No copying allowed
		ThreadPool(ThreadPool const&) = delete;
		ThreadPool& operator=(ThreadPool const&) = delete;		
	};


	template<typename FunctionType>
	std::future<typename std::invoke_result<FunctionType>::type> ThreadPool::EnqueueJob(FunctionType job)
	{
		typedef typename std::invoke_result<FunctionType>::type resultType;

		std::packaged_task<resultType()> packagedTask(std::move(job));
		std::future<resultType> taskFuture(packagedTask.get_future());

		// Add the task to our queue:
		size_t currentJobQueueSize = 0;
		{
			std::unique_lock<std::mutex> waitingLock(m_jobQueueMutex);
			m_jobQueue.push(std::move(packagedTask));
			currentJobQueueSize = m_jobQueue.size();
		}

		GrowThreadPool(currentJobQueueSize);

		m_jobQueueCV.notify_one();

		return taskFuture;
	}
}