// � 2022 Adam Badke. All rights reserved.
#pragma once
#include "BindlessResourceManager.h"
#include "BufferAllocator.h"
#include "GPUTimer.h"
#include "RLibrary_Platform.h"
#include "SwapChain.h"

#include "Core/Host/Window.h"

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
	public:
		static std::unique_ptr<re::Context> CreatePlatformContext(
			platform::RenderingAPI, uint8_t numFramesInFlight, host::Window*);
		
		template <typename T>
		T As();


	public:
		virtual ~Context() = default;

		void Create(uint64_t currentFrame);
		void Update(uint64_t currentFrame);
		void Destroy();


	public: // Context interface:
		virtual void Present() = 0;

		virtual re::BindlessResourceManager* GetBindlessResourceManager() = 0;
	private:
		virtual void CreateInternal(uint64_t currentFrame) = 0;
		virtual void UpdateInternal(uint64_t currentFrame) = 0;
		virtual void DestroyInternal() = 0;


	public:
		host::Window* GetWindow() const noexcept;

		re::SwapChain& GetSwapChain();
		re::SwapChain const& GetSwapChain() const noexcept;

		re::BufferAllocator* GetBufferAllocator();
		re::BufferAllocator const* GetBufferAllocator() const;

		platform::RLibrary* GetOrCreateRenderLibrary(platform::RLibrary::Type);

		re::GPUTimer& GetGPUTimer();


	protected:
		Context(platform::RenderingAPI api, uint8_t numFramesInFlight, host::Window*);


	protected:
		std::unique_ptr<re::BufferAllocator> m_bufferAllocator;
		std::array<std::unique_ptr<platform::RLibrary>, platform::RLibrary::Type_Count> m_renderLibraries;

	private:
		host::Window* m_window;
		re::SwapChain m_swapChain;


	protected:
		re::GPUTimer m_gpuTimer;

		uint8_t m_numFramesInFlight;


	public: // RenderDoc debugging
		typedef RENDERDOC_API_1_1_2 RenderDocAPI;
		RenderDocAPI* GetRenderDocAPI() const;

	private:
		RenderDocAPI* m_renderDocApi;
	};


	inline host::Window* Context::GetWindow() const noexcept
	{
		return m_window;
	}


	inline re::SwapChain& Context::GetSwapChain()
	{
		return m_swapChain;
	}


	inline re::SwapChain const& Context::GetSwapChain() const noexcept
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


	inline re::GPUTimer& Context::GetGPUTimer()
	{
		return m_gpuTimer;
	}


	template <typename T>
	inline T Context::As()
	{
		return dynamic_cast<T>(this);
	}


	inline Context::RenderDocAPI* Context::GetRenderDocAPI() const
	{
		return m_renderDocApi;
	}
}