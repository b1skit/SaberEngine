// © 2022 Adam Badke. All rights reserved.
#include <GL/glew.h>
#include <GL/wglew.h> // Windows-specific GL functions and macros

#include "Core\Config.h"
#include "Core\Assert.h"
#include "SwapChain_OpenGL.h"


namespace opengl
{
	void SwapChain::Create(re::SwapChain& swapChain)
	{
		opengl::SwapChain::PlatformParams* swapChainParams = 
			swapChain.GetPlatformParams()->As<opengl::SwapChain::PlatformParams*>();

		// Default target set:
		LOG("Creating default texure target set");
		swapChainParams->m_backbufferTargetSet = re::TextureTargetSet::Create("Backbuffer");

		const re::TextureTarget::TargetParams::BlendModes backbufferBlendModes
		{
			re::TextureTarget::TargetParams::BlendMode::One,
			re::TextureTarget::TargetParams::BlendMode::Zero
		};
		swapChainParams->m_backbufferTargetSet->SetColorTargetBlendModes(1, &backbufferBlendModes);

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