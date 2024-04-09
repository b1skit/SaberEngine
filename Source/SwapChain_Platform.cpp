// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "SwapChain_DX12.h"
#include "SwapChain_OpenGL.h"
#include "SwapChain_Platform.h"


namespace platform
{
	void SwapChain::CreatePlatformParams(re::SwapChain& swapChain)
	{
		const platform::RenderingAPI api = en::Config::Get()->GetRenderingAPI();

		switch (api)
		{
		case platform::RenderingAPI::OpenGL:
		{
			swapChain.SetPlatformParams(std::make_unique<opengl::SwapChain::PlatformParams>());
		}
		break;
		case platform::RenderingAPI::DX12:
		{
			swapChain.SetPlatformParams(std::make_unique<dx12::SwapChain::PlatformParams>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void (*platform::SwapChain::Create)(re::SwapChain&) = nullptr;
	void (*platform::SwapChain::Destroy)(re::SwapChain&) = nullptr;
	void (*platform::SwapChain::SetVSyncMode)(re::SwapChain const& swapChain, bool enabled) = nullptr;
}
