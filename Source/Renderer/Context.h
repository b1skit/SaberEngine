// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "AccelerationStructureManager.h"
#include "BufferAllocator.h"
#include "GPUTimer.h"
#include "RLibrary_Platform.h"
#include "SwapChain.h"

#include "Core/Host/Window.h"

#include "Core/Interfaces/IPlatformParams.h"

#include "renderdoc_app.h"


namespace opengl
{
	class Context;
}

namespace dx12
{
	class Context;
}

namespace re
{
	class Context
	{
	public: // Singleton functionality
		static Context* Get(); 
		
		template <typename T>
		static T GetAs();


	public:
		virtual ~Context() = default;

		void Create(uint64_t currentFrame);
		void Destroy();


	public: // Context interface:
		virtual void Present() = 0;
	private:
		virtual void CreateInternal(uint64_t currentFrame) = 0;
	

	public:
		void SetWindow(host::Window*); // Set once
		host::Window* GetWindow() const;

		re::SwapChain& GetSwapChain();
		re::SwapChain const& GetSwapChain() const;

		re::BufferAllocator* GetBufferAllocator();
		re::BufferAllocator const* GetBufferAllocator() const;

		platform::RLibrary* GetOrCreateRenderLibrary(platform::RLibrary::Type);
		
		void CreateAccelerationStructureManager();
		void UpdateAccelerationStructureManager(); // Helper: No-op if ASM not created
		re::AccelerationStructureManager* GetAccelerationStructureManager() const;

		re::GPUTimer& GetGPUTimer();


	private:
		static std::unique_ptr<re::Context> CreateSingleton();

	protected:
		Context();


	protected:
		std::unique_ptr<re::BufferAllocator> m_bufferAllocator;
		std::array<std::unique_ptr<platform::RLibrary>, platform::RLibrary::Type_Count> m_renderLibraries;
		std::unique_ptr<re::AccelerationStructureManager> m_ASManager;


	private:
		host::Window* m_window;
		re::SwapChain m_swapChain;


	protected:
		re::GPUTimer m_gpuTimer;

		
	public: // RenderDoc debugging
		typedef RENDERDOC_API_1_1_2 RenderDocAPI;
		RenderDocAPI* GetRenderDocAPI() const;

	private:
		RenderDocAPI* m_renderDocApi;
	};


	inline void Context::SetWindow(host::Window* window)
	{
		SEAssert(window != nullptr, "Trying to set a null window. This is unexpected");
		SEAssert(m_window == nullptr, "Trying to re-set the window. This is unexpected");
		m_window = window;
	}


	inline host::Window* Context::GetWindow() const
	{
		return m_window;
	}


	inline re::SwapChain& Context::GetSwapChain()
	{
		return m_swapChain;
	}


	inline re::SwapChain const& Context::GetSwapChain() const
	{
		return m_swapChain;
	}


	inline re::BufferAllocator* Context::GetBufferAllocator()
	{
		SEAssert(m_bufferAllocator && m_bufferAllocator->IsValid(), "Buffer allocator has already been destroyed");
		return m_bufferAllocator.get();
	}


	inline re::BufferAllocator const* Context::GetBufferAllocator() const
	{
		SEAssert(m_bufferAllocator && m_bufferAllocator->IsValid(), "Buffer allocator has already been destroyed");
		return m_bufferAllocator.get();
	}


	inline void Context::UpdateAccelerationStructureManager()
	{
		if (m_ASManager)
		{
			m_ASManager->Update();
		}
	}


	inline re::AccelerationStructureManager* Context::GetAccelerationStructureManager() const
	{
		return m_ASManager.get();
	}


	inline re::GPUTimer& Context::GetGPUTimer()
	{
		return m_gpuTimer;
	}


	template <typename T>
	inline T Context::GetAs()
	{
		return dynamic_cast<T>(re::Context::Get());
	}


	inline Context::RenderDocAPI* Context::GetRenderDocAPI() const
	{
		return m_renderDocApi;
	}
}