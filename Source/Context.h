// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "TextureTarget.h"
#include "PipelineState.h"


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
		
		std::shared_ptr<re::TextureTargetSet> GetBackbufferTextureTargetSet() const { return m_backbuffer; }

		Context::PlatformParams* const GetPlatformParams() const { return m_platformParams.get(); }
		void SetPlatformParams(std::unique_ptr<Context::PlatformParams> params) { m_platformParams = std::move(params); }

		// Platform wrappers:
		void Create();
		void Destroy();

		void Present() const;
		void SetVSyncMode(bool enabled) const;

		void SetPipelineState(gr::PipelineState const& pipelineState);
		
		// Static platform wrappers:
		static uint32_t GetMaxTextureInputs();		

	private:
		// Note: We store this as a shared_ptr so we can instantiate it once the context has been created
		std::shared_ptr<re::TextureTargetSet> m_backbuffer;
		
		std::unique_ptr<Context::PlatformParams> m_platformParams;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline Context::PlatformParams::~PlatformParams() {};
}