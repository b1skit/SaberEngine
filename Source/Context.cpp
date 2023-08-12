// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "Context.h"
#include "Context_DX12.h"
#include "Context_OpenGL.h"
#include "Context_Platform.h"
#include "DebugConfiguration.h"
#include "SysInfo_Platform.h"

using std::make_shared;


namespace re
{
	Context* Context::Get()
	{
		static std::unique_ptr<re::Context> instance = std::move(re::Context::CreateSingleton());
		return instance.get();
	}


	std::unique_ptr<re::Context> Context::CreateSingleton()
	{
		std::unique_ptr<re::Context> newContext = nullptr;
		const platform::RenderingAPI& api = en::Config::Get()->GetRenderingAPI();
		switch (api)
		{
		case platform::RenderingAPI::OpenGL:
		{
			newContext.reset(new opengl::Context());
		}
		break;
		case platform::RenderingAPI::DX12:
		{
			newContext.reset(new dx12::Context());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}

		return newContext;
	}


	void Context::Destroy()
	{
		m_swapChain.Destroy();
		platform::Context::Destroy(*this);
	}
}