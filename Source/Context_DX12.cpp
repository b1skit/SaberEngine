// © 2022 Adam Badke. All rights reserved.
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


	Context::Context()
	{
	}


	void Context::Create(re::Context& context)
	{
		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		EnableDebugLayer();

		ctxPlatParams->m_device.Create();

		
		// TODO: Move command queue management to its own object?
		// TODO: Support command queues of different types (direct/copy/compute/etc)
		ctxPlatParams->m_commandQueue.Create(ctxPlatParams->m_device.GetDisplayDevice(), D3D12_COMMAND_LIST_TYPE_DIRECT);


		
		// NOTE: Currently, this call retrieves m_commandQueue from the Context platform params
		// TODO: Clean this up, it's gross. Command queue should be its own object
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

		for (uint32_t i = 0; i < dx12::RenderManager::k_numFrames; ++i)
		{
			ctxPlatParams->m_commandAllocators[i] = 
				CreateCommandAllocator(ctxPlatParams->m_device.GetDisplayDevice(), D3D12_COMMAND_LIST_TYPE_DIRECT);
		}

		ctxPlatParams->m_commandList.Create(ctxPlatParams->m_device.GetDisplayDevice(), 
			ctxPlatParams->m_commandAllocators[swapChainParams->m_backBufferIdx],
			D3D12_COMMAND_LIST_TYPE_DIRECT);

		// Fence:
		ctxPlatParams->m_fence.Create(ctxPlatParams->m_device.GetDisplayDevice());


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
		ctxPlatParams->m_fence.Flush(ctxPlatParams->m_commandQueue.GetD3DCommandQueue().Get(), ctxPlatParams->m_fenceValue);
		ctxPlatParams->m_fence.Destroy();

		ctxPlatParams->m_device.Destroy();
	}


	void Context::Present(re::Context const& context)
	{
		// TODO: Replace all of these direct accesss via the platform params with dx12-layer getters/setters

		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		dx12::SwapChain::PlatformParams* const swapChainPlatParams =
			dynamic_cast<dx12::SwapChain::PlatformParams*>(context.GetSwapChain().GetPlatformParams());

		uint8_t& backbufferIdx = swapChainPlatParams->m_backBufferIdx;

		ComPtr<ID3D12Resource>& backBuffer = swapChainPlatParams->m_backBuffers[backbufferIdx];

		// First, we must transition our backbuffer to the present state:
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBuffer.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT); // Note: D3D12_RESOURCE_STATE_PRESENT == D3D12_RESOURCE_STATE_COMMON
		ctxPlatParams->m_commandList.AddResourceBarrier(1, &barrier);

		// Close the command list before we execute it
		ctxPlatParams->m_commandList.Close();

		// Build an array of command lists, and execute them:
		ID3D12CommandList* const commandLists[] = {
			ctxPlatParams->m_commandList.GetD3DCommandList().Get()
		};
		ctxPlatParams->m_commandQueue.Execute(_countof(commandLists), commandLists);


		// Present the backbuffer:
		const bool vsyncEnabled = swapChainPlatParams->m_vsyncEnabled;
		UINT syncInterval = vsyncEnabled ? 1 : 0;
		UINT presentFlags = swapChainPlatParams->m_tearingSupported && !vsyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0;

		HRESULT hr = swapChainPlatParams->m_swapChain->Present(syncInterval, presentFlags);
		CheckHResult(hr, "Failed to present backbuffer");

		// Insert a signal into the command queue:
		ctxPlatParams->m_frameFenceValues[backbufferIdx] = 
			ctxPlatParams->m_fence.Signal(ctxPlatParams->m_commandQueue.GetD3DCommandQueue(), ctxPlatParams->m_fenceValue);

		// Update the index of our current backbuffer
		// Note: Backbuffer indices are not guaranteed to be sequential if we're using DXGI_SWAP_EFFECT_FLIP_DISCARD
		backbufferIdx = swapChainPlatParams->m_swapChain->GetCurrentBackBufferIndex();
		
		// Wait on our fence (blocking)
		ctxPlatParams->m_fence.WaitForGPU(ctxPlatParams->m_frameFenceValues[backbufferIdx]);
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


	ComPtr<ID3D12CommandQueue> Context::CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
	{
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		ComPtr<ID3D12CommandQueue> cmdQueue;

		D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
		cmdQueueDesc.Type		= type; // Direct, compute, copy, etc
		cmdQueueDesc.Priority	= D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; 
		cmdQueueDesc.Flags		= D3D12_COMMAND_QUEUE_FLAG_NONE; // None, or Disable Timeout
		cmdQueueDesc.NodeMask	= deviceNodeMask;

		HRESULT hr = device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&cmdQueue));
		CheckHResult(hr, "Failed to create command queue");

		return cmdQueue;
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


	ComPtr<ID3D12CommandAllocator> Context::CreateCommandAllocator(
		ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
	{
		ComPtr<ID3D12CommandAllocator> commandAllocator;
		HRESULT hr = device->CreateCommandAllocator(
			type, // Copy, compute, direct draw, etc
			IID_PPV_ARGS(&commandAllocator)); // REFIID/GUID (Globally-Unique IDentifier) for the command allocator
		// NOTE: IID_PPV_ARGS macro automatically supplies both the RIID & interface pointer

		CheckHResult(hr, "Failed to create command allocator");

		return commandAllocator;
	}
}