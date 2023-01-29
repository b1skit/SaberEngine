// © 2022 Adam Badke. All rights reserved.
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "CoreEngine.h"
#include "Debug_DX12.h"
#include "DebugConfiguration.h"
#include "SwapChain_DX12.h"
#include "Window_Win32.h"


using Microsoft::WRL::ComPtr;


namespace
{
	// TODO: Figure out why creation fails for D3D_FEATURE_LEVEL_12_2
	constexpr D3D_FEATURE_LEVEL k_targetFeatureLevel = D3D_FEATURE_LEVEL_12_1;
}


namespace dx12
{
	void Context::Create(re::Context& context)
	{
		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		EnableDebugLayer();

		// Find the display adapter with the most VRAM:
		ctxPlatParams->m_dxgiAdapter4 = GetDisplayAdapter();

		// Create a device from the selected adapter:
		ctxPlatParams->m_device = CreateDevice(ctxPlatParams->m_dxgiAdapter4);


		// TODO: Move command queue management to its own object?
		// TODO: Support command queues of different types (direct/copy/compute/etc)
		ctxPlatParams->m_commandQueue = 
			CreateCommandQueue(ctxPlatParams->m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);


		
		// NOTE: Currently, this call retrieves m_commandQueue from the Context platform params
		// TODO: Clean this up, it's gross. Command queue should be its own object
		context.GetSwapChain().Create();
		
		dx12::SwapChain::PlatformParams* const swapChainParams =
			dynamic_cast<dx12::SwapChain::PlatformParams*>(context.GetSwapChain().GetPlatformParams());
		


		ctxPlatParams->m_RTVDescHeap = CreateDescriptorHeap(
			ctxPlatParams->m_device, 
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 
			swapChainParams->m_numBuffers);
		
		ctxPlatParams->m_RTVDescSize = 
			ctxPlatParams->m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		UpdateRenderTargetViews(
			ctxPlatParams->m_device, 
			swapChainParams->m_swapChain,
			swapChainParams->m_backBuffers,
			swapChainParams->m_numBuffers,
			ctxPlatParams->m_RTVDescHeap);

		for (int i = 0; i < swapChainParams->m_numBuffers; ++i)
		{
			ctxPlatParams->m_commandAllocators[i] = 
				CreateCommandAllocator(ctxPlatParams->m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
		}

		ctxPlatParams->m_commandList = CreateCommandList(
			ctxPlatParams->m_device,
			ctxPlatParams->m_commandAllocators[swapChainParams->m_backBufferIdx],
			D3D12_COMMAND_LIST_TYPE_DIRECT);

		ctxPlatParams->m_fence = CreateFence(ctxPlatParams->m_device);
		ctxPlatParams->m_fenceEvent = CreateEventHandle();

		en::Window* window = en::CoreEngine::Get()->GetWindow();
		SEAssert("Window pointer cannot be null", window);
		win32::Window::PlatformParams* const windowPlatParams =
			dynamic_cast<win32::Window::PlatformParams*>(window->GetPlatformParams());


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
			ctxPlatParams->m_device.Get(),
			swapChainParams->m_numBuffers, // Number of frames in flight
			swapChainParams->m_displayFormat,
			ctxPlatParams->m_RTVDescHeap.Get(),
			ctxPlatParams->m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
			ctxPlatParams->m_RTVDescHeap->GetGPUDescriptorHandleForHeapStart());
	}


	void Context::Destroy(re::Context& context)
	{
		dx12::Context::PlatformParams* const ctxPlatParams =
			dynamic_cast<dx12::Context::PlatformParams*>(context.GetPlatformParams());

		// Make sure the command queue has finished all commands before closing.
		Flush(ctxPlatParams->m_commandQueue, ctxPlatParams->m_fence, ctxPlatParams->m_fenceValue, ctxPlatParams->m_fenceEvent);

		::CloseHandle(ctxPlatParams->m_fenceEvent);

		ctxPlatParams->m_dxgiAdapter4->Release();
	}


	void Context::Present(re::Context const& context)
	{
		SEAssertF("TODO: Implement this");
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


	ComPtr<IDXGIAdapter4> Context::GetDisplayAdapter()
	{
		// Create a DXGI factory object
		ComPtr<IDXGIFactory4> dxgiFactory;
		uint32_t createFactoryFlags = 0;
#if defined(_DEBUG)
		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
		
		HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
		CheckHResult(hr, "Failed to create DXGIFactory2");

		ComPtr<IDXGIAdapter1> dxgiAdapter1;
		ComPtr<IDXGIAdapter4> dxgiAdapter4;

		// Query each of our HW adapters:
		size_t maxVRAM = 0;
		for (uint32_t i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);
			
			const size_t vram = dxgiAdapterDesc1.DedicatedVideoMemory / (1024u * 1024u);
			LOG(L"Querying adapter %d: %s, %ju MB VRAM", dxgiAdapterDesc1.DeviceId, dxgiAdapterDesc1.Description, vram);

			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
				SUCCEEDED(
					D3D12CreateDevice(dxgiAdapter1.Get(), k_targetFeatureLevel, __uuidof(ID3D12Device), nullptr)) &&
				dxgiAdapterDesc1.DedicatedVideoMemory > maxVRAM)
			{
				maxVRAM = dxgiAdapterDesc1.DedicatedVideoMemory;

				hr = dxgiAdapter1.As(&dxgiAdapter4);
				CheckHResult(hr, "Failed to cast selected dxgiAdapter4 to dxgiAdapter1");
			}
		}

		return dxgiAdapter4;
	}


	ComPtr<ID3D12Device2> Context::CreateDevice(ComPtr<IDXGIAdapter4> adapter)
	{
		ComPtr<ID3D12Device2> d3d12Device2;
		HRESULT hr = D3D12CreateDevice(adapter.Get(), k_targetFeatureLevel, IID_PPV_ARGS(&d3d12Device2));
		CheckHResult(hr, "Failed to create device");

#if defined(_DEBUG)
		ComPtr<ID3D12InfoQueue> infoQueue;
		if (SUCCEEDED(d3d12Device2.As(&infoQueue)))
		{
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

			// Suppress message categories
			//D3D12_MESSAGE_CATEGORY Categories[] = {};

			// Suppress messages by severity level
			D3D12_MESSAGE_SEVERITY severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO
			};

			// Suppress individual messages by ID
			D3D12_MESSAGE_ID denyIds[] = 
			{
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,	// No idea how to avoid this message yet
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,							// Occurs when using capture frame while graphics debugging
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,						// Occurs when using capture frame while graphics debugging
			};

			D3D12_INFO_QUEUE_FILTER newFilter = {};
			//NewFilter.DenyList.NumCategories = _countof(Categories);
			//NewFilter.DenyList.pCategoryList = Categories;
			newFilter.DenyList.NumSeverities = _countof(severities);
			newFilter.DenyList.pSeverityList = severities;
			newFilter.DenyList.NumIDs = _countof(denyIds);
			newFilter.DenyList.pIDList = denyIds;

			hr = infoQueue->PushStorageFilter(&newFilter);
			CheckHResult(hr, "Failed to push storage filter");
		}
#endif

		return d3d12Device2;
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


	ComPtr<ID3D12GraphicsCommandList> Context::CreateCommandList(
		ComPtr<ID3D12Device2> device,
		ComPtr<ID3D12CommandAllocator> cmdAllocator, 
		D3D12_COMMAND_LIST_TYPE type)
	{
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		ComPtr<ID3D12GraphicsCommandList> commandList;

		HRESULT hr = device->CreateCommandList(
			deviceNodeMask,
			type, // Direct draw/compute/copy/etc
			cmdAllocator.Get(), // The command allocator the command lists will be created on
			nullptr,  // Optional: Command list initial pipeline state
			IID_PPV_ARGS(&commandList)); // REFIID/GUID of the command list interface, & destination for the populated command list
		// NOTE: IID_PPV_ARGS macro automatically supplies both the RIID & interface pointer

		CheckHResult(hr, "Failed to create command list");

		// Note: Command lists are created in the recording state by default. The render loop resets the command list,
		// which requires the command list to be closed. So, we pre-close new command lists so they're ready to be reset 
		// before recording
		hr = commandList->Close();
		CheckHResult(hr, "Failed to close command list");

		return commandList;
	}


	ComPtr<ID3D12Fence> Context::CreateFence(ComPtr<ID3D12Device2> device)
	{
		ComPtr<ID3D12Fence> fence;
		HRESULT hr = device->CreateFence(
			0, // Initial value: It's recommended that fences always start at 0, and increase monotonically ONLY
			D3D12_FENCE_FLAG_NONE, // Fence flags: Shared, cross-adapter, etc
			IID_PPV_ARGS(&fence)); // REFIIF and destination pointer for the populated fence
		
		CheckHResult(hr, "Failed to create fence");

		return fence;
	}


	HANDLE Context::CreateEventHandle()
	{
		HANDLE fenceEvent = ::CreateEvent(
			NULL, // Pointer to event SECURITY_ATTRIBUTES. If null, the handle cannot be inherited by child processes
			FALSE, // Manual reset? If true, event must be reset to non-signalled by calling ResetEvent. Auto-resets if false
			FALSE, // Initial state: true/false = signalled/unsignalled
			NULL); // Event object name: Unnamed if null

		SEAssert("Failed to create fence event", fenceEvent);

		return fenceEvent;
	}


	void Context::Flush(
		ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,	uint64_t& fenceValue, HANDLE fenceEvent)
	{
		uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
		WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
	}


	uint64_t Context::Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue)
	{
		uint64_t fenceValueForSignal = ++fenceValue;

		HRESULT hr = commandQueue->Signal(
			fence.Get(), // Fence object ptr
			fenceValueForSignal); // Value to signal the fence with
		
		CheckHResult(hr, "Failed to signal fence");

		return fenceValueForSignal;
	}


	void Context::WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent)
	{
		constexpr std::chrono::milliseconds duration = std::chrono::milliseconds::max();

		if (fence->GetCompletedValue() < fenceValue)
		{
			HRESULT hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);
			CheckHResult(hr, "Failed to set completion event");

			::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
		}
	}
}