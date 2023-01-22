// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "Context.h"
#include "Context_Platform.h"
#include "DebugConfiguration.h"



namespace re
{
	using std::make_shared;

	Context::Context()
		: m_backbuffer(nullptr)
	{
		platform::Context::CreatePlatformParams(*this);
	}


	void Context::Create()
	{
		platform::Context::Create(*this);

		// Default target set:
		LOG("Creating default texure target set");
		m_backbuffer = make_shared<re::TextureTargetSet>("Backbuffer");
		m_backbuffer->Viewport() =
		{
			0,
			0,
			(uint32_t)en::Config::Get()->GetValue<int>("windowXRes"),
			(uint32_t)en::Config::Get()->GetValue<int>("windowYRes")
		};
		// Note: Default framebuffer has no texture targets
	}


	void Context::Destroy()
	{
		m_backbuffer = nullptr;
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


	uint32_t Context::GetMaxTextureInputs()
	{
		return platform::Context::GetMaxTextureInputs();
	}
}