// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Core/CommandQueue.h"


namespace re
{
	class RenderManager;
}
namespace pr
{
	// Graphics service interface. This is used to interface with GraphicsSystems over a command queue, which guarantees
	// commands are executed single threaded at the beginning of each frame
	class IGraphicsService
	{
	public:
		virtual ~IGraphicsService() = default;

		void Initialize(core::CommandManager*);


	protected:
		template<typename T, typename... Args>
		void EnqueueServiceCommand(Args&&... args);

		void EnqueueServiceCommand(std::function<void(void)>&&);


	private:
		virtual void DoInitialize() = 0;


	private:
		core::CommandManager* m_commandQueue = nullptr;
	};


	inline void IGraphicsService::Initialize(core::CommandManager* renderCmdQueue)
	{
		m_commandQueue = renderCmdQueue;

		DoInitialize();
	}


	template<typename T, typename... Args>
	void IGraphicsService::EnqueueServiceCommand(Args&&... args)
	{
		SEAssert(m_commandQueue != nullptr, "Command queue is null, was Initialize() called?");
		m_commandQueue->Enqueue<T>(std::forward<Args>(args)...);
	}


	inline void IGraphicsService::EnqueueServiceCommand(std::function<void(void)>&& cmd)
	{
		SEAssert(m_commandQueue != nullptr, "Command queue is null, was Initialize() called?");
		m_commandQueue->Enqueue(std::forward<std::function<void(void)>>(cmd));
	}
}