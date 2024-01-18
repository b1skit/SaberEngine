// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "renderdoc_app.h"

#include "IPlatformParams.h"
#include "TextureTarget.h"
#include "ParameterBlockAllocator.h"
#include "PipelineState.h"
#include "SwapChain.h"


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

		re::ParameterBlockAllocator& GetParameterBlockAllocator();
		re::ParameterBlockAllocator const& GetParameterBlockAllocator() const;


	private:
		static std::unique_ptr<re::Context> CreateSingleton();

	protected:
		Context();


	protected:
		re::ParameterBlockAllocator m_paramBlockAllocator;


	private:
		re::SwapChain m_swapChain;

		
	public: // RenderDoc debugging
		typedef RENDERDOC_API_1_1_2 RenderDocAPI;
		RenderDocAPI* GetRenderDocAPI() const;

	private:
		RenderDocAPI* m_renderDocApi;
	};


	inline re::ParameterBlockAllocator& Context::GetParameterBlockAllocator()
	{
		SEAssert(m_paramBlockAllocator.IsValid(), "Parameter block allocator has already been destroyed");
		return m_paramBlockAllocator;
	}


	inline re::ParameterBlockAllocator const& Context::GetParameterBlockAllocator() const
	{
		SEAssert(m_paramBlockAllocator.IsValid(), "Parameter block allocator has already been destroyed");
		return m_paramBlockAllocator;
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