// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "Context.h"
#include "Context_Platform.h"
#include "DebugConfiguration.h"



namespace re
{
	using std::make_shared;

	Context::Context()
	{
		platform::Context::CreatePlatformParams(*this);
	}


	void Context::Create()
	{
		platform::Context::Create(*this);
	}


	void Context::Destroy()
	{
		m_swapChain.Destroy();
		platform::Context::Destroy(*this);
	}


	void Context::Present() const
	{
		platform::Context::Present(*this);
	}


	void Context::SetVSyncMode(bool enabled) const
	{
		platform::Context::SetVSyncMode(*this, enabled);
	}


	void Context::SetPipelineState(gr::PipelineState const& pipelineState)
	{
		platform::Context::SetPipelineState(*this, pipelineState);
	}


	uint8_t Context::GetMaxTextureInputs() const
	{
		return platform::Context::GetMaxTextureInputs();
	}


	uint8_t Context::GetMaxColorTargets() const
	{
		return platform::Context::GetMaxColorTargets();
	}
}