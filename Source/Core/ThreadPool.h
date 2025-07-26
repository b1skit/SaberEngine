// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace core
{
	class FunctionWrapper final
	{
	public:
		template<typename Function>
		FunctionWrapper(Function&& function) : m_impl(new ImplType<Function>(std::forward<Function>(function)))  {}
	
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
			ImplType(Function&& function) noexcept : m_function(std::forward<Function>(function)) {}

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
		static void Startup();
		static void Stop();

		template<typename FunctionType>
		static std::future<typename std::invoke_result<FunctionType>::type> EnqueueJob(FunctionType job); // Producer

		static void NameCurrentThread(wchar_t const* threadName);

	private:
		static void ExecuteJobs(); // Consumer loop

		static void AddWorkerThread();


	private:
		static bool s_isRunning;

		static std::mutex s_jobQueueMutex;
		static std::condition_variable s_jobQueueCV;

		static std::queue<FunctionWrapper> s_jobQueue;

		static std::vector<std::thread> s_workerThreads;


	private: // Static class only
		ThreadPool() = delete;
		ThreadPool(ThreadPool&&) noexcept = delete;
		ThreadPool(ThreadPool const&) = delete;
		ThreadPool& operator=(ThreadPool&&) noexcept = delete;		
		ThreadPool& operator=(ThreadPool const&) = delete;
		~ThreadPool() = default;
	};


	template<typename FunctionType>
	std::future<typename std::invoke_result<FunctionType>::type> ThreadPool::EnqueueJob(FunctionType job)
	{
		typedef typename std::invoke_result<FunctionType>::type resultType;

		std::packaged_task<resultType()> packagedTask(std::move(job));
		std::future<resultType> taskFuture(packagedTask.get_future());

		// Add the task to our queue:
		{
			std::unique_lock<std::mutex> waitingLock(s_jobQueueMutex);
			s_jobQueue.push(std::move(packagedTask));
		}

		s_jobQueueCV.notify_one();

		return taskFuture;
	}
}