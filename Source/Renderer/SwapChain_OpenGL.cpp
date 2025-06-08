// � 2022 Adam Badke. All rights reserved.
#include "TextureTarget.h"
#include "SwapChain_OpenGL.h"

#include "Core/Assert.h"
#include "Core/Config.h"


namespace opengl
{
	void SwapChain::Create(re::SwapChain& swapChain, re::Texture::Format format)
	{
		opengl::SwapChain::PlatObj* swapChainParams = 
			swapChain.GetPlatformObject()->As<opengl::SwapChain::PlatObj*>();

		swapChainParams->m_backbufferDimensions = glm::uvec2(
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey)),
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey)));

		swapChainParams->m_backbufferFormat = format;

		// Default target set:
		LOG("Creating default texure target set");
		swapChainParams->m_backbufferTargetSet = re::TextureTargetSet::Create("Backbuffer");

		swapChainParams->m_backbufferTargetSet->SetViewport(
		{
			0,
			0,
			swapChainParams->m_backbufferDimensions.x,
			swapChainParams->m_backbufferDimensions.y
		});
		// Note: OpenGL framebuffer has no texture targets		
	}


	void SwapChain::Destroy(re::SwapChain& swapChain)
	{
		opengl::SwapChain::PlatObj* swapChainParams = 
			swapChain.GetPlatformObject()->As<opengl::SwapChain::PlatObj*>();
		if (!swapChainParams)
		{
			return;
		}

		swapChainParams->m_backbufferTargetSet = nullptr;
	}


	bool SwapChain::ToggleVSync(re::SwapChain const& swapChain)
	{
		opengl::SwapChain::PlatObj* swapChainParams =
			swapChain.GetPlatformObject()->As<opengl::SwapChain::PlatObj*>();

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
		opengl::SwapChain::PlatObj* swapChainParams =
			swapChain.GetPlatformObject()->As<opengl::SwapChain::PlatObj*>();
		SEAssert(swapChainParams && swapChainParams->m_backbufferTargetSet,
			"Swap chain params and backbuffer cannot be null");

		return swapChainParams->m_backbufferTargetSet;
	}


	re::Texture::Format SwapChain::GetBackbufferFormat(re::SwapChain const& swapChain)
	{
		opengl::SwapChain::PlatObj const* platObj =
			swapChain.GetPlatformObject()->As<opengl::SwapChain::PlatObj const*>();

		SEAssert(platObj->m_backbufferFormat != re::Texture::Format::Invalid, "Swapchain is not correctly configured");

		return platObj->m_backbufferFormat;
	}


	glm::uvec2 SwapChain::GetBackbufferDimensions(re::SwapChain const& swapChain)
	{
		opengl::SwapChain::PlatObj const* platObj =
			swapChain.GetPlatformObject()->As<opengl::SwapChain::PlatObj const*>();

		SEAssert(platObj->m_backbufferDimensions.x > 0 && platObj->m_backbufferDimensions.y > 0,
			"Swapchain is not correctly configured");

		return platObj->m_backbufferDimensions;
	}
}