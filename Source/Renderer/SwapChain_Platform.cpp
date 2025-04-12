// © 2022 Adam Badke. All rights reserved.
#include "SwapChain_DX12.h"
#include "SwapChain_OpenGL.h"
#include "SwapChain_Platform.h"
#include "TextureTarget.h"

#include "Core/Config.h"


namespace platform
{
	void SwapChain::CreatePlatformObject(re::SwapChain& swapChain)
	{
		const platform::RenderingAPI api = re::RenderManager::Get()->GetRenderingAPI();

		switch (api)
		{
		case platform::RenderingAPI::OpenGL:
		{
			swapChain.SetPlatformObject(std::make_unique<opengl::SwapChain::PlatObj>());
		}
		break;
		case platform::RenderingAPI::DX12:
		{
			swapChain.SetPlatformObject(std::make_unique<dx12::SwapChain::PlatObj>());
		}
		break;
		default:
		{
			SEAssertF("Invalid rendering API argument received");
		}
		}
	}


	void (*SwapChain::Create)(re::SwapChain&) = nullptr;
	void (*SwapChain::Destroy)(re::SwapChain&) = nullptr;
	bool (*SwapChain::ToggleVSync)(re::SwapChain const& swapChain) = nullptr;
	std::shared_ptr<re::TextureTargetSet>(*SwapChain::GetBackBufferTargetSet)(re::SwapChain const&) = nullptr;
}
