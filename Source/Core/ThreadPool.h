// � 2022 Adam Badke. All rights reserved.
#pragma once


namespace core
{
	class FunctionWrapper final
	{
	public:
		template<typename Function>
		FunctionWrapper(Function&& function) : m_impl(new ImplType<Function>(std::forward<Function>(function))) {}
	
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
			ImplType(Function&& function) : m_function(std::forward<Function>(function)) {}

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


	class ThreadPool final
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

		void AddWorkerThread();


	private:
		bool m_isRunning;

		std::mutex m_jobQueueMutex;
		std::condition_variable m_jobQueueCV;

		std::queue<FunctionWrapper> m_jobQueue;

		std::vector<std::thread> m_workerThreads;


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
		{
			std::unique_lock<std::mutex> waitingLock(m_jobQueueMutex);
			m_jobQueue.push(std::move(packagedTask));
		}

		m_jobQueueCV.notify_one();

		return taskFuture;
	}
}