// � 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h
#include <dxgi1_6.h>

#include "Config.h"
#include "Context_DX12.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "RenderManager.h"
#include "SwapChain_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture.h"
#include "Texture_DX12.h"
#include "Window_Win32.h"

using Microsoft::WRL::ComPtr;


namespace
{
	using dx12::CheckHResult;


	void CreateSwapChainTargetSet(
		dx12::SwapChain::PlatformParams* swapChainParams, 
		re::Texture::TextureParams const& colorParams,
		re::Texture::TextureParams const& depthParams)
	{
		dx12::Context::PlatformParams* ctxPlatParams = 
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		// TEMP HAX!!!
		// TODO: GET AN RTV HANDLE IN A LESS BRITTLE WAY!
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(ctxPlatParams->m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart());


		for (uint8_t backbufferIdx = 0; backbufferIdx < dx12::RenderManager::k_numFrames; backbufferIdx++)
		{
			// Target set:
			swapChainParams->m_backbufferTargetSets[backbufferIdx] =
				std::make_shared<re::TextureTargetSet>(std::string("Backbuffer_%s", backbufferIdx));

			ComPtr<ID3D12Resource> backbufferResource;
			HRESULT hr = swapChainParams->m_swapChain->GetBuffer(backbufferIdx, IID_PPV_ARGS(&backbufferResource));
			CheckHResult(hr, "Failed to get backbuffer");

			// Color target:
			std::shared_ptr<re::Texture> colorTargetTex = 
				std::make_shared<re::Texture>("SwapChainColorTarget", colorParams);
			dx12::Texture::CreateFromExistingResource(*colorTargetTex, backbufferResource, rtvHandle);

			swapChainParams->m_backbufferTargetSets[backbufferIdx]->SetColorTarget(0, colorTargetTex);

			dx12::TextureTargetSet::CreateColorTargets(*swapChainParams->m_backbufferTargetSets[backbufferIdx]);


			// TEMP HAX!!!
			// TODO: REQUEST ALL THE VIEWS WE NEED AT ONCE!!!!!
			rtvHandle.Offset(ctxPlatParams->m_RTVDescSize); // Internally strides to the next descriptor


			// Depth target:
			std::shared_ptr<re::Texture> depthTargetTex = 
				std::make_shared<re::Texture>("SwapChainDepthTarget", depthParams);
			dx12::Texture::Create(*depthTargetTex);

			swapChainParams->m_backbufferTargetSets[backbufferIdx]->SetDepthStencilTarget(depthTargetTex);
			
			dx12::TextureTargetSet::CreateDepthStencilTarget(*swapChainParams->m_backbufferTargetSets[backbufferIdx]);

			// Set default viewports and scissor rects. Note: This is NOT required, just included for clarity
			swapChainParams->m_backbufferTargetSets[backbufferIdx]->Viewport() = re::Viewport(); // Defaults = 0, 0, xRes, yRes
			swapChainParams->m_backbufferTargetSets[backbufferIdx]->ScissorRect() = re::ScissorRect(); // Defaults = 0, 0, long::max, long::max
		}		
	}
}

namespace dx12
{
	void SwapChain::Create(re::SwapChain& swapChain)
	{
		dx12::SwapChain::PlatformParams* swapChainParams =
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();
		
		// By default, prefer tearing enable and vsync disabled (best for variable refresh displays)
		swapChainParams->m_tearingSupported = SwapChain::CheckTearingSupport();
		swapChainParams->m_vsyncEnabled = !swapChainParams->m_tearingSupported;

		UINT createFactoryFlags = 0;
#if defined(_DEBUG)
		// Catch errors during device creation. Should not be used in release builds
		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

		ComPtr<IDXGIFactory4> dxgiFactory4;
		HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4));
		CheckHResult(hr, "Failed to create DXGIFactory2");

		// TODO: The context (currently) calls this function. We shouldn't be calling the context from here
		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		const int width = en::Config::Get()->GetValue<int>(en::Config::k_windowXResValueName);
		const int height = en::Config::Get()->GetValue<int>(en::Config::k_windowYResValueName);


		re::Texture::TextureParams colorParams;
		colorParams.m_width = width;
		colorParams.m_height = height;
		colorParams.m_faces = 1;
		colorParams.m_usage = re::Texture::Usage::ColorTarget;
		colorParams.m_dimension = re::Texture::Dimension::Texture2D;
		colorParams.m_format = re::Texture::Format::RGBA8;
		colorParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		colorParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		colorParams.m_useMIPs = false;


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
		swapChainDesc.BufferCount = dx12::RenderManager::k_numFrames; // # buffers (>= 2), including the front buffer
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH; // Resize behavior when back-buffer size != output target size
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // How to handle buffer contents after presenting a surface
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // Back-buffer transparency behavior
		swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;


		SEAssert("Window cannot be null", en::CoreEngine::Get()->GetWindow());
		win32::Window::PlatformParams* windowPlatParams =
			en::CoreEngine::Get()->GetWindow()->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		// Create the swap chain:
		ComPtr<IDXGISwapChain1> swapChain1;
		hr = dxgiFactory4->CreateSwapChainForHwnd(
			ctxPlatParams->m_commandQueues[CommandQueue_DX12::Direct].GetD3DCommandQueue(),
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


		re::Texture::TextureParams depthParams;
		depthParams.m_width = width;
		depthParams.m_height = height;
		depthParams.m_faces = 1;
		depthParams.m_usage = re::Texture::Usage::DepthTarget;
		depthParams.m_dimension = re::Texture::Dimension::Texture2D;
		depthParams.m_format = re::Texture::Format::Depth32F;
		depthParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		depthParams.m_clearColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		depthParams.m_useMIPs = false;

		// Create our target set textures:
		CreateSwapChainTargetSet(swapChainParams, colorParams, depthParams);
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

		for (uint8_t backbuffer = 0; backbuffer < dx12::RenderManager::k_numFrames; backbuffer++)
		{
			swapChainParams->m_backBuffers[backbuffer] = nullptr;
			swapChainParams->m_backbufferTargetSets[backbuffer] = nullptr;
		}
	}


	bool SwapChain::CheckTearingSupport()
	{
		int allowTearing = 0;

		ComPtr<IDXGIFactory5> factory5;

		HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&factory5));
		CheckHResult(hr, "Failed to create DXGI Factory");

		hr = factory5->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&allowTearing,
			sizeof(allowTearing));
		CheckHResult(hr, "Failed to check feature support");

		return allowTearing > 0;
	}


	void SwapChain::SetVSyncMode(re::SwapChain const& swapChain, bool enabled)
	{
		dx12::SwapChain::PlatformParams* swapchainParams =
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		swapchainParams->m_vsyncEnabled = enabled;
	}


	uint8_t SwapChain::GetBackBufferIdx(re::SwapChain const& swapChain)
	{
		dx12::SwapChain::PlatformParams* swapChainPlatParams = 
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		return swapChainPlatParams->m_backBufferIdx;
	}


	Microsoft::WRL::ComPtr<ID3D12Resource> SwapChain::GetBackBufferResource(re::SwapChain const& swapChain)
	{
		dx12::SwapChain::PlatformParams* swapChainPlatParams = 
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		dx12::Texture::PlatformParams const* backbufferColorTexPlatParams = 
			swapChainPlatParams->m_backbufferTargetSets[swapChainPlatParams->m_backBufferIdx]->GetColorTarget(0).GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		return backbufferColorTexPlatParams->m_textureResource;
	}
}