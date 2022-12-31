// © 2022 Adam Badke. All rights reserved.
#include "Context.h"
#include "Context_Platform.h"


namespace re
{
	Context::Context()
	{
		platform::Context::PlatformParams::CreatePlatformParams(*this);
	}

	void Context::Create()
	{
		platform::Context::Create(*this);
	}

	void Context::Destroy()
	{
		platform::Context::Destroy(*this);
	}

	void Context::Present() const
	{
		platform::Context::Present(*this);
	}

	void Context::SetCullingMode(platform::Context::FaceCullingMode const& mode) const
	{
		platform::Context::SetCullingMode(mode);
	}

	void Context::ClearTargets(platform::Context::ClearTarget const& clearTarget) const
	{
		platform::Context::ClearTargets(clearTarget);
	}

	void Context::SetBlendMode(platform::Context::BlendMode const& src, platform::Context::BlendMode const& dst) const
	{
		platform::Context::SetBlendMode(src, dst);
	}

	void Context::SetDepthTestMode(platform::Context::DepthTestMode const& mode) const
	{
		platform::Context::SetDepthTestMode(mode);
	}

	void Context::SetDepthWriteMode(platform::Context::DepthWriteMode const& mode) const
	{
		platform::Context::SetDepthWriteMode(mode);
	}

	void Context::SetColorWriteMode(platform::Context::ColorWriteMode const& channelModes) const
	{
		platform::Context::SetColorWriteMode(channelModes);
	}

	uint32_t Context::GetMaxTextureInputs()
	{
		return platform::Context::GetMaxTextureInputs();
	}


	bool Context::WindowHasFocus() const
	{
		return platform::Context::WindowHasFocus(*this);
	}
}