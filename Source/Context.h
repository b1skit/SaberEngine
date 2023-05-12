// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "IPlatformParams.h"
#include "TextureTarget.h"
#include "ParameterBlockAllocator.h"
#include "PipelineState.h"
#include "SwapChain.h"


namespace re
{
	static constexpr char k_imguiIniPath[] = "config\\imgui.ini";


	class Context
	{
	public:
		struct PlatformParams : public re::IPlatformParams
		{
			PlatformParams() = default;
			virtual ~PlatformParams() = 0;

			// Copying not allowed
			PlatformParams(PlatformParams const&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams const&) = delete;			
		};


	public:
		Context();
		~Context() { Destroy(); }

		re::SwapChain& GetSwapChain() { return m_swapChain; }
		re::SwapChain const& GetSwapChain() const { return m_swapChain; }

		Context::PlatformParams* GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<Context::PlatformParams> params) { m_platformParams = std::move(params); }

		inline re::ParameterBlockAllocator& GetParameterBlockAllocator();
		inline re::ParameterBlockAllocator const& GetParameterBlockAllocator() const;

		// Platform wrappers:
		void Create();
		void Destroy();

		void Present() const;
		
		// Platform wrappers:
		uint8_t GetMaxTextureInputs() const;
		uint8_t GetMaxColorTargets() const;

	private:
		re::SwapChain m_swapChain;

		re::ParameterBlockAllocator m_paramBlockAllocator;
		
		std::unique_ptr<Context::PlatformParams> m_platformParams;
	};


	inline re::ParameterBlockAllocator& Context::GetParameterBlockAllocator()
	{
		SEAssert("Parameter block allocator has already been destroyed", m_paramBlockAllocator.IsValid());
		return m_paramBlockAllocator;
	}


	inline re::ParameterBlockAllocator const& Context::GetParameterBlockAllocator() const
	{
		SEAssert("Parameter block allocator has already been destroyed", m_paramBlockAllocator.IsValid());
		return m_paramBlockAllocator;
	}


	// We need to provide a destructor implementation since it's pure virtual
	inline Context::PlatformParams::~PlatformParams() {};
}