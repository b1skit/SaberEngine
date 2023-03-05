// © 2022 Adam Badke. All rights reserved.
#include <GL/glew.h>
#include <GL/wglew.h> // Windows-specific GL functions and macros

#include "Config.h"
#include "DebugConfiguration.h"
#include "SwapChain_OpenGL.h"


namespace opengl
{
	void SwapChain::Create(re::SwapChain& swapChain)
	{
		opengl::SwapChain::PlatformParams* swapChainParams = 
			swapChain.GetPlatformParams()->As<opengl::SwapChain::PlatformParams*>();

		// Default target set:
		LOG("Creating default texure target set");
		swapChainParams->m_backbufferTargetSet = std::make_shared<re::TextureTargetSet>("Backbuffer");
		swapChainParams->m_backbufferTargetSet->Viewport() =
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
		opengl::SwapChain::PlatformParams* swapChainParams = 
			swapChain.GetPlatformParams()->As<opengl::SwapChain::PlatformParams*>();

		if (!swapChainParams)
		{
			return;
		}

		swapChainParams->m_backbufferTargetSet = nullptr;
	}


	void SwapChain::SetVSyncMode(re::SwapChain const& swapChain, bool enabled)
	{
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
			wglSwapIntervalEXT(static_cast<int8_t>(enabled)); // # frames of delay: VSync == 1
		}
		else
		{
			SEAssertF("VSync extension not supported");
		}
	}
}