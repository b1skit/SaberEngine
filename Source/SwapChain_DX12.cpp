// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "Context_DX12.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "RenderManager.h"
#include "SwapChain_DX12.h"
#include "Window_Win32.h"


namespace dx12
{
	using Microsoft::WRL::ComPtr;


	void SwapChain::Create(re::SwapChain& swapChain)
	{
		dx12::SwapChain::PlatformParams* const swapChainParams =
			dynamic_cast<dx12::SwapChain::PlatformParams*>(swapChain.GetPlatformParams());

		// By default, prefer tearing enable and vsync disabled (best for variable refresh displays)
		swapChainParams->m_tearingSupported = SwapChain::CheckTearingSupport();
		swapChainParams->m_vsyncEnabled = !swapChainParams->m_tearingSupported;

		SEAssert("Window cannot be null", en::CoreEngine::Get()->GetWindow());
		win32::Window::PlatformParams* const windowPlatParams =
			dynamic_cast<win32::Window::PlatformParams*>(en::CoreEngine::Get()->GetWindow()->GetPlatformParams());

		const int width = en::Config::Get()->GetValue<int>(en::Config::k_windowXResValueName);
		const int height = en::Config::Get()->GetValue<int>(en::Config::k_windowYResValueName);


		// TODO: The context (currently) calls this function. We shouldn't be calling the context from here
		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(re::RenderManager::Get()->GetContext().GetPlatformParams());


		UINT createFactoryFlags = 0;
#if defined(_DEBUG)
		// Catch errors during device creation. Should not be used in release builds
		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

		ComPtr<IDXGIFactory4> dxgiFactory4;
		HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4));
		CheckHResult(hr, "Failed to create DXGIFactory2");

		// Create our swap chain description:
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Display format
		swapChainDesc.Stereo = FALSE; // We're not creating a stereo swap chain
		swapChainDesc.SampleDesc = { 1, 0 }; // Mandatory value if NOT using a DX11-style bitblt swap chain
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // Specify back-buffer surface usage and CPU access
		swapChainDesc.BufferCount = swapChainParams->m_numBuffers; // # buffers (>= 2), including the front buffer
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH; // Resize behavior when back-buffer size != output target size
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // How to handle buffer contents after presenting a surface
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // Back-buffer transparency behavior
		swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

		// Create the swap chain:
		ComPtr<IDXGISwapChain1> swapChain1;
		hr = dxgiFactory4->CreateSwapChainForHwnd(
			ctxPlatParams->m_commandQueue.Get(), // Pointer to direct command queue
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
	}


	void SwapChain::Destroy(re::SwapChain& swapChain)
	{
		SEAssertF("TODO: Implement this");
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
		dx12::SwapChain::PlatformParams* const swapchainParams =
			dynamic_cast<dx12::SwapChain::PlatformParams*>(swapChain.GetPlatformParams());

		swapchainParams->m_vsyncEnabled = enabled;
	}
}