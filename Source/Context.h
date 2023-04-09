// � 2022 Adam Badke. All rights reserved.
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
		struct PlatformParams : public IPlatformParams
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

		inline re::ParameterBlockAllocator& GetParameterBlockAllocator() { return m_paramBlockAllocator; }
		inline re::ParameterBlockAllocator const& GetParameterBlockAllocator() const { return m_paramBlockAllocator; }

		// Platform wrappers:
		void Create();
		void Destroy();

		void Present() const;

		void SetPipelineState(gr::PipelineState const& pipelineState);
		
		// Platform wrappers:
		uint8_t GetMaxTextureInputs() const;
		uint8_t GetMaxColorTargets() const;

	private:
		re::SwapChain m_swapChain;

		re::ParameterBlockAllocator m_paramBlockAllocator;
		
		std::unique_ptr<Context::PlatformParams> m_platformParams;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline Context::PlatformParams::~PlatformParams() {};
}