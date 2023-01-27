// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "TextureTarget.h"
#include "PipelineState.h"
#include "SwapChain.h"


namespace re
{
	static constexpr char k_imguiIniPath[] = "config\\imgui.ini";


	class Context
	{
	public:
		struct PlatformParams
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

		re::SwapChain& GetSwapChain() { return m_swapChain; }
		re::SwapChain const& GetSwapChain() const { return m_swapChain; }

		Context::PlatformParams* const GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<Context::PlatformParams> params) { m_platformParams = std::move(params); }

		// Platform wrappers:
		void Create();
		void Destroy();

		void Present() const;
		void SetVSyncMode(bool enabled) const;

		void SetPipelineState(gr::PipelineState const& pipelineState);
		
		// Platform wrappers:
		uint8_t GetMaxTextureInputs() const;
		uint8_t GetMaxColorTargets() const;

	private:
		re::SwapChain m_swapChain;
		
		std::unique_ptr<Context::PlatformParams> m_platformParams;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline Context::PlatformParams::~PlatformParams() {};
}