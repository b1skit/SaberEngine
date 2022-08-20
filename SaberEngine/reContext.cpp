#include "reContext.h"
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


	void Context::SwapWindow()
	{
		platform::Context::SwapWindow(*this);
	}
}