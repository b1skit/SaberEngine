// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "Context.h"
#include "Context_Platform.h"
#include "DebugConfiguration.h"


namespace re
{
	using std::make_shared;

	Context::Context()
		: m_platformParams(nullptr)
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
		m_platformParams = nullptr;
	}


	void Context::Present() const
	{
		platform::Context::Present(*this);
	}


	uint8_t Context::GetMaxTextureInputs() const
	{
		return platform::Context::GetMaxTextureInputs();
	}
}