// © 2022 Adam Badke. All rights reserved.
#include "SwapChain_OpenGL.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include <GL/glew.h>
#include <GL/wglew.h>


namespace opengl
{
	void SwapChain::Create(re::SwapChain& swapChain)
	{
		opengl::SwapChain::PlatformParams* swapChainParams = 
			swapChain.GetPlatformParams()->As<opengl::SwapChain::PlatformParams*>();

		// Default target set:
		LOG("Creating default texure target set");
		swapChainParams->m_backbufferTargetSet = re::TextureTargetSet::Create("Backbuffer");

		swapChainParams->m_backbufferTargetSet->SetViewport(
		{
			0,
			0,
			(uint32_t)core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey),
			(uint32_t)core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey)
		});
		// Note: OpenGL framebuffer has no texture targets
	}


	void SwapChain::Destroy(re::SwapChain& swapChain)
	{
		opengl::SwapChain::PlatformParams* swapChainParams = 
			swapChain.GetPlatformParams()->As<opengl::SwapChain::PlatformParams*>();
		if (!swapChainParams)
		{
			return;
		}

		swapChainParams->m_backbufferTargetSet = nullptr;
	}


	bool SwapChain::ToggleVSync(re::SwapChain const& swapChain)
	{
		opengl::SwapChain::PlatformParams* swapChainParams =
			swapChain.GetPlatformParams()->As<opengl::SwapChain::PlatformParams*>();

		swapChainParams->m_vsyncEnabled = !swapChainParams->m_vsyncEnabled;

		// Based on the technique desecribed here:
		// https://stackoverflow.com/questions/589064/how-to-enable-vertical-sync-in-opengl
		auto WGLExtensionSupported = [](const char* extension_name)
			{
				// Wgl function pointer, gets a string with list of wgl extensions:
				PFNWGLGETEXTENSIONSSTRINGEXTPROC _wglGetExtensionsStringEXT =
					(PFNWGLGETEXTENSIONSSTRINGEXTPROC)wglGetProcAddress("wglGetExtensionsStringEXT");

				if (::strstr(_wglGetExtensionsStringEXT(), extension_name) == nullptr)
				{
					return false; // Extension not found/supported
				}

				return true; // Extension supported
			};

		PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;
		if (WGLExtensionSupported("WGL_EXT_swap_control"))
		{
			wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
			wglSwapIntervalEXT(static_cast<int8_t>(swapChainParams->m_vsyncEnabled)); // 0/1: VSync disabled/enabled
		}
		else
		{
			SEAssertF("VSync extension not supported");
		}

		return swapChainParams->m_vsyncEnabled;
	}


	std::shared_ptr<re::TextureTargetSet> SwapChain::GetBackBufferTargetSet(re::SwapChain const& swapChain)
	{
		opengl::SwapChain::PlatformParams* swapChainParams =
			swapChain.GetPlatformParams()->As<opengl::SwapChain::PlatformParams*>();
		SEAssert(swapChainParams && swapChainParams->m_backbufferTargetSet,
			"Swap chain params and backbuffer cannot be null");

		return swapChainParams->m_backbufferTargetSet;
	}
}