// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "DebugConfiguration.h"
#include "SwapChain_OpenGL.h"


namespace opengl
{
	void SwapChain::Create(re::SwapChain& swapChain)
	{
		opengl::SwapChain::PlatformParams* const swapChainParams =
			dynamic_cast<opengl::SwapChain::PlatformParams*>(swapChain.GetPlatformParams());

		// Default target set:
		LOG("Creating default texure target set");
		swapChainParams->m_backbuffer = std::make_shared<re::TextureTargetSet>("Backbuffer");
		swapChainParams->m_backbuffer->Viewport() =
		{
			0,
			0,
			(uint32_t)en::Config::Get()->GetValue<int>(en::Config::k_windowXResValueName),
			(uint32_t)en::Config::Get()->GetValue<int>(en::Config::k_windowYResValueName)
		};
		// Note: Default framebuffer has no texture targets
	}


	void SwapChain::Destroy(re::SwapChain& swapChain)
	{
		opengl::SwapChain::PlatformParams* const swapChainParams =
			dynamic_cast<opengl::SwapChain::PlatformParams*>(swapChain.GetPlatformParams());

		swapChainParams->m_backbuffer = nullptr;
	}
}