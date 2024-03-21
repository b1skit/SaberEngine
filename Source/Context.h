// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "IPlatformParams.h"
#include "TextureTarget.h"
#include "BufferAllocator.h"
#include "PipelineState.h"
#include "SwapChain.h"

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
		virtual ~Context() = 0;

		// Context interface:
		[[nodiscard]] virtual void Create(uint64_t currentFrame) = 0;
		virtual void Present() = 0;

		// Platform wrappers:
		void Destroy();


		re::SwapChain& GetSwapChain() { return m_swapChain; }
		re::SwapChain const& GetSwapChain() const { return m_swapChain; }

		re::BufferAllocator* GetBufferAllocator();
		re::BufferAllocator const* GetBufferAllocator() const;


	private:
		static std::unique_ptr<re::Context> CreateSingleton();

	protected:
		Context();


	protected:
		std::unique_ptr<re::BufferAllocator> m_bufferAllocator;


	private:
		re::SwapChain m_swapChain;

		
	public: // RenderDoc debugging
		typedef RENDERDOC_API_1_1_2 RenderDocAPI;
		RenderDocAPI* GetRenderDocAPI() const;

	private:
		RenderDocAPI* m_renderDocApi;
	};


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


	template <typename T>
	inline T Context::GetAs()
	{
		return dynamic_cast<T>(re::Context::Get());
	}


	inline Context::RenderDocAPI* Context::GetRenderDocAPI() const
	{
		return m_renderDocApi;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Context::~Context() {};
}