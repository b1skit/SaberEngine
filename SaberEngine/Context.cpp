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

	void Context::SwapWindow() const
	{
		platform::Context::SwapWindow(*this);
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
}