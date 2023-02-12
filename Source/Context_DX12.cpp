// � 2022 Adam Badke. All rights reserved.
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "CoreEngine.h"
#include "Debug_DX12.h"
#include "DebugConfiguration.h"
#include "RenderManager_DX12.h"
#include "SwapChain_DX12.h"
#include "Window_Win32.h"


namespace dx12
{
	using Microsoft::WRL::ComPtr;


	Context::PlatformParams::PlatformParams()
		: m_RTVDescHeap(nullptr)
		, m_RTVDescSize(0)
	{
		for (uint32_t i = 0; i < dx12::RenderManager::k_numFrames; i++)
		{
			m_frameFenceValues[i] = 0;
		}
	}


	Context::Context()
	{
	}


	void Context::Create(re::Context& context)
	{
		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		EnableDebugLayer();

		ctxPlatParams->m_device.Create();

		// TODO: Use command queues of different types (direct/copy/compute/etc)
		ctxPlatParams->m_commandQueue.Create(
			ctxPlatParams->m_device.GetDisplayDevice(), 
			D3D12_COMMAND_LIST_TYPE_DIRECT);

		
		// NOTE: Currently, this call retrieves m_commandQueue from the Context platform params
		// TODO: Clean this up, it's gross.
		context.GetSwapChain().Create();
		
		dx12::SwapChain::PlatformParams* const swapChainParams =
			dynamic_cast<dx12::SwapChain::PlatformParams*>(context.GetSwapChain().GetPlatformParams());


		ctxPlatParams->m_RTVDescHeap = CreateDescriptorHeap(
			ctxPlatParams->m_device.GetDisplayDevice(),
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 
			dx12::RenderManager::k_numFrames);
		
		ctxPlatParams->m_RTVDescSize = 
			ctxPlatParams->m_device.GetDisplayDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		UpdateRenderTargetViews(
			ctxPlatParams->m_device.GetDisplayDevice(),
			swapChainParams->m_swapChain,
			swapChainParams->m_backBuffers,
			dx12::RenderManager::k_numFrames,
			ctxPlatParams->m_RTVDescHeap);

		SEAssert("Window pointer cannot be null", en::CoreEngine::Get()->GetWindow());
		win32::Window::PlatformParams* const windowPlatParams =
			dynamic_cast<win32::Window::PlatformParams*>(en::CoreEngine::Get()->GetWindow()->GetPlatformParams());


		// Setup our ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = re::k_imguiIniPath;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup Platform/Renderer backends
		ImGui_ImplWin32_Init(windowPlatParams->m_hWindow);
		ImGui_ImplDX12_Init(
			ctxPlatParams->m_device.GetDisplayDevice().Get(),
			dx12::RenderManager::k_numFrames, // Number of frames in flight
			swapChainParams->m_displayFormat,
			ctxPlatParams->m_RTVDescHeap.Get(),
			ctxPlatParams->m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			ctxPlatParams->m_RTVDescHeap->GetGPUDescriptorHandleForHeapStart());
	}


	void Context::Destroy(re::Context& context)
	{
		// ImGui Cleanup:
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		// Make sure the command queue has finished all commands before closing.
		ctxPlatParams->m_commandQueue.Flush();
		ctxPlatParams->m_commandQueue.Destroy();

		ctxPlatParams->m_device.Destroy();
	}


	void Context::Present(re::Context const& context)
	{
		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		dx12::SwapChain::PlatformParams* const swapChainPlatParams =
			dynamic_cast<dx12::SwapChain::PlatformParams*>(context.GetSwapChain().GetPlatformParams());

		const uint8_t backbufferIdx = swapChainPlatParams->m_backBufferIdx;

		// Present the backbuffer:
		const bool vsyncEnabled = swapChainPlatParams->m_vsyncEnabled;
		const uint32_t syncInterval = vsyncEnabled ? 1 : 0;
		const uint32_t presentFlags = 
			swapChainPlatParams->m_tearingSupported && !vsyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0;

		HRESULT hr = swapChainPlatParams->m_swapChain->Present(syncInterval, presentFlags);
		CheckHResult(hr, "Failed to present backbuffer");

		// Insert a signal into the command queue:
		ctxPlatParams->m_frameFenceValues[backbufferIdx] = ctxPlatParams->m_commandQueue.Signal();
		// TODO: We should maintain a frame fence, and individual fences per command queue

		// Get the next backbuffer index:
		// Note: Backbuffer indices are not guaranteed to be sequential if we're using DXGI_SWAP_EFFECT_FLIP_DISCARD
		swapChainPlatParams->m_backBufferIdx = swapChainPlatParams->m_swapChain->GetCurrentBackBufferIndex();
		
		// Wait on the fence for our new backbuffer index, to ensure the GPU is finished with it (blocking)
		ctxPlatParams->m_commandQueue.WaitForGPU(ctxPlatParams->m_frameFenceValues[backbufferIdx]);
	}


	void Context::SetPipelineState(re::Context const& context, gr::PipelineState const& pipelineState)
	{
		SEAssertF("TODO: Implement this");
	}


	uint8_t Context::GetMaxTextureInputs()
	{
		SEAssertF("TODO: Implement this");
		return 0;
	}


	uint8_t Context::GetMaxColorTargets()
	{
		return D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}


	ComPtr<ID3D12DescriptorHeap> Context::CreateDescriptorHeap(
		ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors)
	{
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		ComPtr<ID3D12DescriptorHeap> descriptorHeap;

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = type; // What's in our heap? CBV/SRV/UAV, sampler, RTV, DSV
		desc.NumDescriptors = numDescriptors;
		//desc.Flags = ; // TODO: Do we need any specific flags?
		desc.NodeMask = deviceNodeMask;

		HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
		CheckHResult(hr, "Failed to create descriptor heap");

		return descriptorHeap;
	}


	void Context::UpdateRenderTargetViews(
		ComPtr<ID3D12Device2> device,
		ComPtr<IDXGISwapChain4> swapChain, 
		ComPtr<ID3D12Resource>* buffers, 
		uint8_t numBuffers, 
		ComPtr<ID3D12DescriptorHeap> descriptorHeap)
	{
		// The size of a single descriptor is vendor-specific, so we retrieve it here:
		uint32_t rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// Get a pointer/handle to the 1st descriptor in our heap:
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

		for (uint8_t i = 0; i < numBuffers; ++i)
		{
			// Get a pointer to the back-buffer:
			ComPtr<ID3D12Resource> backBuffer;
			HRESULT hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
			CheckHResult(hr, "Failed to get backbuffer");

			// Create the RTV:
			device->CreateRenderTargetView(
				backBuffer.Get(), // Pointer to the resource containing the render target texture
				nullptr,  // Pointer to a render target view descriptor. nullptr = default
				rtvHandle); // Descriptor destination

			buffers[i] = backBuffer; // Store the backbuffer pointer obtained from the swap chain

			rtvHandle.Offset(rtvDescriptorSize); // Stride to the next descriptor
		}
	}
}