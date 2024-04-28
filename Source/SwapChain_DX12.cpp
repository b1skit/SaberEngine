// © 2022 Adam Badke. All rights reserved.
#include <d3dx12.h>
#include <dxgi1_6.h>

#include "Config.h"
#include "Context_DX12.h"
#include "EngineApp.h"
#include "Assert.h"
#include "Debug_DX12.h"
#include "RenderManager.h"
#include "SwapChain_DX12.h"
#include "SysInfo_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture.h"
#include "Texture_DX12.h"
#include "Window_Win32.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	void SwapChain::Create(re::SwapChain& swapChain)
	{
		dx12::SwapChain::PlatformParams* swapChainParams =
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();
		
		swapChainParams->m_backbufferTargetSets.resize(dx12::RenderManager::GetNumFramesInFlight(), nullptr);

		// Ideally, tearing should be enabled and vsync disabled (best for variable refresh displays), but we respect
		// the config
		swapChainParams->m_tearingSupported = SysInfo::CheckTearingSupport();
		swapChainParams->m_vsyncEnabled = en::Config::Get()->GetValue<bool>("vsync");

		UINT createFactoryFlags = 0;
#if defined(_DEBUG)
		// Catch errors during device creation. Should not be used in release builds
		if (en::Config::Get()->GetValue<int>(en::ConfigKeys::k_debugLevelCmdLineArg) > 0)
		{
			createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
		}
#endif

		ComPtr<IDXGIFactory4> dxgiFactory4;
		HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4));
		CheckHResult(hr, "Failed to create DXGIFactory2");

		const int width = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
		const int height = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);

		re::Texture::TextureParams colorParams;
		colorParams.m_width = width;
		colorParams.m_height = height;
		colorParams.m_faces = 1;
		colorParams.m_usage = re::Texture::Usage::SwapchainColorProxy;
		colorParams.m_dimension = re::Texture::Dimension::Texture2D;
		colorParams.m_format = re::Texture::Format::RGBA8_UNORM;
		colorParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		colorParams.m_mipMode = re::Texture::MipMode::None;
		colorParams.m_addToSceneData = false;
		colorParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		// Ensure our format here matches the one that our texture will be created with:
		const DXGI_FORMAT colorBufferFormat = dx12::Texture::GetTextureFormat(colorParams);

		// Create our swap chain description:
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = colorBufferFormat;
		swapChainDesc.Stereo = FALSE; // We're not creating a stereo swap chain
		swapChainDesc.SampleDesc = { 1, 0 }; // Mandatory value if NOT using a DX11-style bitblt swap chain
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // Specify back-buffer surface usage and CPU access
		swapChainDesc.BufferCount = dx12::RenderManager::GetNumFramesInFlight(); // # buffers (>= 2), including the front buffer
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH; // Resize behavior when back-buffer size != output target size
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // How to handle buffer contents after presenting a surface
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // Back-buffer transparency behavior
		swapChainDesc.Flags = swapChainParams->m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;


		SEAssert(en::EngineApp::Get()->GetWindow(), "Window cannot be null");
		win32::Window::PlatformParams* windowPlatParams =
			en::EngineApp::Get()->GetWindow()->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		// Note: The context (currently) calls this function. This is dicey...
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		// Create the swap chain:
		ComPtr<IDXGISwapChain1> swapChain1;
		hr = dxgiFactory4->CreateSwapChainForHwnd(
			context->GetCommandQueue(dx12::CommandListType::Direct).GetD3DCommandQueue(),
			windowPlatParams->m_hWindow, // Window handle associated with the swap chain
			&swapChainDesc, // Swap chain descriptor
			nullptr, // Full-screen swap chain descriptor. Creates a window swap chain if null
			nullptr, // Pointer to an interface that content should be restricted to. Content is unrestricted if null
			&swapChain1); // Output: Our created swap chain
		CheckHResult(hr, "Failed to create swap chain");

		// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen will be handled manually
		hr = dxgiFactory4->MakeWindowAssociation(windowPlatParams->m_hWindow, DXGI_MWA_NO_ALT_ENTER);
		CheckHResult(hr, "Failed to make window association");

		hr = swapChain1.As(&swapChainParams->m_swapChain);
		CheckHResult(hr, "Failed to convert swap chain"); // Convert IDXGISwapChain1 -> IDXGISwapChain4
 
		swapChainParams->m_backBufferIdx = swapChainParams->m_swapChain->GetCurrentBackBufferIndex();


		// Create the depth target texture:
		re::Texture::TextureParams depthParams;
		depthParams.m_width = width;
		depthParams.m_height = height;
		depthParams.m_faces = 1;
		depthParams.m_usage = re::Texture::Usage::DepthTarget;
		depthParams.m_dimension = re::Texture::Dimension::Texture2D;
		depthParams.m_format = re::Texture::Format::Depth32F;
		depthParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		depthParams.m_mipMode = re::Texture::MipMode::None;
		depthParams.m_addToSceneData = false;
		depthParams.m_clear.m_depthStencil.m_depth = 1.f; // Far plane

		std::shared_ptr<re::Texture> depthTargetTex = re::Texture::Create("SwapChainDepthTarget", depthParams);

		re::TextureTarget::TargetParams depthTargetParams;

		// Create color target textures, attach them to our target set, & copy the backbuffer resource into their
		// platform params:
		for (uint8_t backbufferIdx = 0; backbufferIdx < dx12::RenderManager::GetNumFramesInFlight(); backbufferIdx++)
		{
			// Create a target set to hold our backbuffer targets:
			swapChainParams->m_backbufferTargetSets[backbufferIdx] = 
				re::TextureTargetSet::Create("BackbufferTargetSet_" + std::to_string(backbufferIdx));

			// Set the shared depth buffer texture:
			swapChainParams->m_backbufferTargetSets[backbufferIdx]->SetDepthStencilTarget(depthTargetTex, depthTargetParams);

			// Get the pre-existing backbuffer resource from the swapchain:
			ComPtr<ID3D12Resource> backbufferResource;
			HRESULT hr = swapChainParams->m_swapChain->GetBuffer(backbufferIdx, IID_PPV_ARGS(&backbufferResource));
			CheckHResult(hr, "Failed to get backbuffer");

			// Create (and name) a color target texture:
			std::shared_ptr<re::Texture> colorTargetTex = dx12::Texture::CreateFromExistingResource(
				"SwapChainColorTarget_" + std::to_string(backbufferIdx), 
				colorParams, 
				backbufferResource);

			re::TextureTarget::TargetParams targetParams;

			swapChainParams->m_backbufferTargetSets[backbufferIdx]->SetColorTarget(0, colorTargetTex, targetParams);

			SEAssert(colorTargetTex->GetPlatformParams()->As<dx12::Texture::PlatformParams*>()->m_format == colorBufferFormat,
				"Unexpected texture format selected");

			// Set default viewports and scissor rects. Note: This is NOT required, just included for clarity
			swapChainParams->m_backbufferTargetSets[backbufferIdx]->SetViewport(
				re::Viewport()); // Defaults = 0, 0, xRes, yRes
			
			swapChainParams->m_backbufferTargetSets[backbufferIdx]->SetScissorRect(
				re::ScissorRect()); // Defaults = 0, 0, long::max, long::max
		}
	}


	void SwapChain::Destroy(re::SwapChain& swapChain)
	{
		dx12::SwapChain::PlatformParams* swapChainParams =
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();
		if (!swapChainParams)
		{
			return;
		}

		// Must exit fullscreen before releasing the swapchain
		HRESULT hr = swapChainParams->m_swapChain->SetFullscreenState(false, NULL);

		for (uint8_t backbuffer = 0; backbuffer < dx12::RenderManager::GetNumFramesInFlight(); backbuffer++)
		{
			swapChainParams->m_backbufferTargetSets[backbuffer] = nullptr;
		}
	}


	void SwapChain::SetVSyncMode(re::SwapChain const& swapChain, bool enabled)
	{
		dx12::SwapChain::PlatformParams* swapchainParams =
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		swapchainParams->m_vsyncEnabled = enabled;
	}


	uint8_t SwapChain::GetCurrentBackBufferIdx(re::SwapChain const& swapChain)
	{
		dx12::SwapChain::PlatformParams* swapChainPlatParams = 
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		return swapChainPlatParams->m_backBufferIdx;
	}


	uint8_t SwapChain::IncrementBackBufferIdx(re::SwapChain& swapChain)
	{
		dx12::SwapChain::PlatformParams* swapChainPlatParams =
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		// Note: Backbuffer indices are not guaranteed to be sequential if we're using  DXGI_SWAP_EFFECT_FLIP_DISCARD
		swapChainPlatParams->m_backBufferIdx = swapChainPlatParams->m_swapChain->GetCurrentBackBufferIndex();
		
		return swapChainPlatParams->m_backBufferIdx;
	}


	std::shared_ptr<re::TextureTargetSet> SwapChain::GetBackBufferTargetSet(re::SwapChain const& swapChain)
	{
		dx12::SwapChain::PlatformParams* swapChainPlatParams =
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		const uint8_t backbufferIdx = swapChainPlatParams->m_backBufferIdx;

		return swapChainPlatParams->m_backbufferTargetSets[backbufferIdx];
	}
}