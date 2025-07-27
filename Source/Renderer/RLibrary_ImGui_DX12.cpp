// © 2024 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "RLibrary_ImGui_DX12.h"
#include "SwapChain_DX12.h"
#include "SysInfo_DX12.h"
#include "Texture_DX12.h"

#include "Core/Logger.h"
#include "Core/ProfilingMarkers.h"

#include "Core/Host/Window_Win32.h"

#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"


using Microsoft::WRL::ComPtr;


namespace
{
	re::Context* g_context = nullptr; // Internal global context pointer for ImGui callbacks
}

namespace dx12
{
	void RLibraryImGui::PlatObj::InitializeImGuiSRVHeap()
	{
		SEBeginCPUEvent("RLibraryImGui::PlatObj::InitializeImGuiSRVHeap");

		util::ScopedThreadProtector scopedThreadProtector(m_threadProtector);
		
		dx12::Context* context = GetContext()->As<dx12::Context*>();
		Microsoft::WRL::ComPtr<ID3D12Device> device = context->GetDevice().GetD3DDevice();

		const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {
			.Type = k_heapType,
			.NumDescriptors = k_imguiHeapSize,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = dx12::SysInfo::GetDeviceNodeMask() };

		HRESULT hr = device->CreateDescriptorHeap(
			&descriptorHeapDesc, IID_PPV_ARGS(&m_imGuiGPUVisibleSRVDescriptorHeap));
		CheckHResult(hr, "Failed to create a descriptor heap for ImGui SRVs");

		m_imGuiGPUVisibleSRVDescriptorHeap->SetName(L"ImGui descriptor heap");

		m_heapStartCPU = m_imGuiGPUVisibleSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		m_heapStartGPU = m_imGuiGPUVisibleSRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		m_handleIncrementSize = device->GetDescriptorHandleIncrementSize(k_heapType);

		// Initialize tracking:
		m_freeIndices.reserve(k_imguiHeapSize);
		for (int32_t i = k_imguiHeapSize - 1; i >= 0; --i)
		{
			m_freeIndices.emplace_back(i); // Add in reverse order, so our allocations start at 0
		}
		SEStaticAssert(k_imguiHeapSize < std::numeric_limits<int32_t>::max(), "Heap size will overflow this loop");

		SEEndCPUEvent(); // "RLibraryImGui::PlatObj::InitializeImGuiSRVHeap"
	}


	void RLibraryImGui::PlatObj::DestroyImGuiSRVHeap()
	{
		SEBeginCPUEvent("RLibraryImGui::PlatObj::DestroyImGuiSRVHeap");

		util::ScopedThreadProtector scopedThreadProtector(m_threadProtector);

		SEAssert(m_freeIndices.size() == k_imguiHeapSize, "Missing ImGui free indices - have all been returned?");

		m_imGuiGPUVisibleSRVDescriptorHeap = nullptr;
		m_heapStartCPU = {};
		m_heapStartGPU = {};
		m_handleIncrementSize = 0;
		m_freeIndices.clear();

		SEEndCPUEvent(); // "RLibraryImGui::PlatObj::DestroyImGuiSRVHeap"
	}


	void RLibraryImGui::PlatObj::Allocate(
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandleOut,
		D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandleOut)
	{
		SEBeginCPUEvent("RLibraryImGui::PlatObj::Allocate");

		dx12::RLibraryImGui* dx12ImGuiLibrary = nullptr;
		if (info) // Might be null if we're calling internally
		{
			dx12ImGuiLibrary = static_cast<dx12::RLibraryImGui*>(info->UserData);
		}
		else
		{
			SEAssert(g_context, "Context pointer is null");

			dx12ImGuiLibrary = dynamic_cast<dx12::RLibraryImGui*>(
				g_context->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));
		}
		SEAssert(dx12ImGuiLibrary, "Failed to get RLibraryImGui");

		dx12::RLibraryImGui::PlatObj* platObj =
			dx12ImGuiLibrary->GetPlatformObject()->As<dx12::RLibraryImGui::PlatObj*>();

		util::ScopedThreadProtector scopedThreadProtector(platObj->m_threadProtector);

		SEAssert(!platObj->m_freeIndices.empty(),
			"No free indices to allocate from. Consider increasing k_imguiHeapSize");

		const uint32_t allocationIdx = platObj->m_freeIndices.back();
		platObj->m_freeIndices.pop_back();

		cpuHandleOut->ptr = platObj->m_heapStartCPU.ptr + (allocationIdx * platObj->m_handleIncrementSize);
		gpuHandleOut->ptr = platObj->m_heapStartGPU.ptr + (allocationIdx * platObj->m_handleIncrementSize);

		SEEndCPUEvent(); // "RLibraryImGui::PlatObj::Allocate"
	}


	void RLibraryImGui::PlatObj::Free(
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHanle)
	{
		SEBeginCPUEvent("RLibraryImGui::PlatObj::Free");

		dx12::RLibraryImGui* dx12ImGuiLibrary = nullptr;
		if (info) // Might be null if we're calling internally
		{
			dx12ImGuiLibrary = static_cast<dx12::RLibraryImGui*>(info->UserData);
		}
		else
		{
			SEAssert(g_context, "Context pointer is null");

			dx12ImGuiLibrary = dynamic_cast<dx12::RLibraryImGui*>(
				g_context->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));
		}
		SEAssert(dx12ImGuiLibrary, "Failed to get RLibraryImGui");

		dx12::RLibraryImGui::PlatObj* platObj =
			dx12ImGuiLibrary->GetPlatformObject()->As<dx12::RLibraryImGui::PlatObj*>();

		util::ScopedThreadProtector scopedThreadProtector(platObj->m_threadProtector);

		const uint32_t cpuIdx = 
			static_cast<uint32_t>((cpuHandle.ptr - platObj->m_heapStartCPU.ptr) / platObj->m_handleIncrementSize);

		SEAssert(cpuIdx == 
			static_cast<uint32_t>((gpuHanle.ptr - platObj->m_heapStartGPU.ptr) / platObj->m_handleIncrementSize),
			"CPU and GPU heap pointers are out of sync");
		
		platObj->m_freeIndices.push_back(cpuIdx);

		SEEndCPUEvent(); // "RLibraryImGui::PlatObj::Free"
	}


	void RLibraryImGui::PlatObj::CopyTempDescriptorToImGuiHeap(
		D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor,
		D3D12_CPU_DESCRIPTOR_HANDLE& cpuDstOut,
		D3D12_GPU_DESCRIPTOR_HANDLE& gpuDstOut)
	{
		SEBeginCPUEvent("RLibraryImGui::PlatObj::CopyTempDescriptorToImGuiHeap");

		// Allocate a destination in our ImGui descriptor heap:
		Allocate(nullptr, &cpuDstOut, &gpuDstOut);

		util::ScopedThreadProtector scopedThreadProtector(m_threadProtector); // Avoid recursive call in Allocate()

		// Copy the descriptor in:
		dx12::Context* context = GetContext()->As<dx12::Context*>();
		Microsoft::WRL::ComPtr<ID3D12Device> device = context->GetDevice().GetD3DDevice();

		const uint32_t numDescriptors = 1;

		device->CopyDescriptors(
			1,					// UINT NumDestDescriptorRanges
			&cpuDstOut,			// const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts
			&numDescriptors,	// const UINT* pDestDescriptorRangeSizes
			1,					// UINT NumSrcDescriptorRanges
			&srcDescriptor,		// const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts
			nullptr,			// const UINT* pSrcDescriptorRangeSizes
			k_heapType);		// D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType

		// Record the allocation in the deferred delete queue: Temporary allocations are valid for a single frame only
		m_deferredDescriptorDelete.emplace(
			context->GetCurrentRenderFrameNum(),
			TempDescriptorAllocation{ 
				cpuDstOut, 
				gpuDstOut });

		SEEndCPUEvent(); // "RLibraryImGui::PlatObj::CopyTempDescriptorToImGuiHeap"
	}


	void RLibraryImGui::PlatObj::FreeTempDescriptors(uint64_t currentFrame)
	{
		SEBeginCPUEvent("RLibraryImGui::PlatObj::FreeTempDescriptors");

		{
			util::ScopedThreadProtector scopedThreadProtector(m_threadProtector); // Avoid recursive call in Free()

			if (m_deferredDescriptorDelete.empty())
			{
				SEEndCPUEvent(); // "RLibraryImGui::PlatObj::FreeTempDescriptors"
				return;
			}
		}

		SEAssert(g_context, "Context pointer is null");

		// Defer deletion by numFramesInFlight
		const uint8_t numFramesInFlight = g_context->GetNumFramesInFlight();

		while (!m_deferredDescriptorDelete.empty() &&
			m_deferredDescriptorDelete.front().first + numFramesInFlight < currentFrame)
		{
			RLibraryImGui::PlatObj::Free(
				nullptr, 
				m_deferredDescriptorDelete.front().second.m_cpuDesc, 
				m_deferredDescriptorDelete.front().second.m_gpuDesc);

			m_deferredDescriptorDelete.pop();
		}

		SEEndCPUEvent(); // "RLibraryImGui::PlatObj::FreeTempDescriptors"
	}


	std::unique_ptr<platform::RLibrary> RLibraryImGui::Create()
	{
		SEBeginCPUEvent("RLibraryImGui::Create");

		std::unique_ptr<platform::RLibrary> newLibrary = std::make_unique<dx12::RLibraryImGui>();

		dx12::RLibraryImGui* dx12ImGuiLibrary = dynamic_cast<dx12::RLibraryImGui*>(newLibrary.get());
		platform::RLibraryImGui::CreateInternal(*dx12ImGuiLibrary);
		
		dx12::Context* context = dx12ImGuiLibrary->GetPlatformObject()->GetContext()->As<dx12::Context*>();
		g_context = context; // Store the context globally for ImGui callbacks
		SEAssert(g_context, "Context pointer is null");

		ID3D12Device* device = context->GetDevice().GetD3DDevice().Get();

		re::SwapChain& swapChain = context->GetSwapChain();

		// Setup ImGui platform/Renderer backends:
		SEAssert(context->GetWindow(), "Window pointer cannot be null");
		win32::Window::PlatObj* windowPlatObj = context->GetWindow()->GetPlatformObject()->As<win32::Window::PlatObj*>();

		dx12::Texture::PlatObj const* backbufferColorTarget0PlatObj =
			dx12::SwapChain::GetBackBufferTargetSet(swapChain)->GetColorTarget(0).GetTexture()
				->GetPlatformObject()->As<dx12::Texture::PlatObj*>();

		::ImGui_ImplWin32_Init(windowPlatObj->m_hWindow);
		::ImGui_ImplWin32_EnableDpiAwareness();

		// Initialize our ImGui descriptor heap (lives in our PlatObj):
		dx12::RLibraryImGui::PlatObj* platObj = 
			dx12ImGuiLibrary->GetPlatformObject()->As<dx12::RLibraryImGui::PlatObj*>();
		
		platObj->InitializeImGuiSRVHeap();

		// ImGui DX12 backend initialization:
		dx12::CommandQueue& directQueue = context->GetCommandQueue(dx12::CommandListType::Direct);

		const uint8_t numFramesInFlight = context->GetNumFramesInFlight();

		ImGui_ImplDX12_InitInfo initInfo{};
		initInfo.Device = device;
		initInfo.CommandQueue = directQueue.GetD3DCommandQueue().Get();
		initInfo.NumFramesInFlight = numFramesInFlight;
		initInfo.RTVFormat = backbufferColorTarget0PlatObj->m_format;
		
		initInfo.SrvDescriptorHeap = platObj->m_imGuiGPUVisibleSRVDescriptorHeap.Get();
		initInfo.SrvDescriptorAllocFn = RLibraryImGui::PlatObj::Allocate;
		initInfo.SrvDescriptorFreeFn = RLibraryImGui::PlatObj::Free;

		// Store our RLibraryImGui pointer so we can access it during alloc/dealloc
		// Note: This is also required as otherwise the alloc/dealloc functions would recursively call this function
		initInfo.UserData = dx12ImGuiLibrary; 

		ImGui_ImplDX12_Init(&initInfo);

		platform::RLibraryImGui::ConfigureScaling(*dynamic_cast<platform::RLibraryImGui*>(newLibrary.get()));

		SEEndCPUEvent(); // "RLibraryImGui::Create"

		return std::move(newLibrary);
	}


	void RLibraryImGui::CopyTempDescriptorToImGuiHeap(
		D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor,
		D3D12_CPU_DESCRIPTOR_HANDLE& cpuDstOut,
		D3D12_GPU_DESCRIPTOR_HANDLE& gpuDstOut)
	{
		SEBeginCPUEvent("RLibraryImGui::CopyTempDescriptorToImGuiHeap");

		SEAssert(g_context, "Context pointer is null");

		dx12::Context* context = g_context->As<dx12::Context*>();

		RLibraryImGui* dx12ImGuiLibrary = dynamic_cast<RLibraryImGui*>(
			context->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));

		dx12::RLibraryImGui::PlatObj* platObj =
			dx12ImGuiLibrary->GetPlatformObject()->As<dx12::RLibraryImGui::PlatObj*>();

		platObj->CopyTempDescriptorToImGuiHeap(srcDescriptor, cpuDstOut, gpuDstOut);

		SEEndCPUEvent(); // "RLibraryImGui::CopyTempDescriptorToImGuiHeap"
	}


	void RLibraryImGui::Destroy()
	{
		SEBeginCPUEvent("RLibraryImGui::Destroy");

		LOG("Destroying ImGui render library");

		SEAssert(g_context, "Context pointer is null");

		// ImGui Cleanup:
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		// Clean up our ImGui descriptor heap:
		RLibraryImGui* dx12ImGuiLibrary = dynamic_cast<RLibraryImGui*>(
			g_context->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));

		dx12::RLibraryImGui::PlatObj* platObj =
			dx12ImGuiLibrary->GetPlatformObject()->As<dx12::RLibraryImGui::PlatObj*>();

		platObj->FreeTempDescriptors(std::numeric_limits<uint64_t>::max());

		platObj->DestroyImGuiSRVHeap();

		g_context = nullptr; // Null out the context for ImGui callbacks

		SEEndCPUEvent(); // "RLibraryImGui::Destroy"
	}


	void RLibraryImGui::Execute(std::unique_ptr<platform::RLibrary::IPayload>&& iPayload, void* platformObject)
	{
		SEBeginCPUEvent("RLibraryImGui::Execute");

		SEBeginCPUEvent("RLibraryImGui::Execute: Setup");

		std::unique_ptr<platform::RLibraryImGui::Payload> payload(
			dynamic_cast<platform::RLibraryImGui::Payload*>(iPayload.release()));

		SEAssert(g_context, "Context pointer is null");
		dx12::Context* context = g_context->As<dx12::Context*>();

		RLibraryImGui* dx12ImGuiLibrary = 
			dynamic_cast<RLibraryImGui*>(context->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));

		SEAssert(payload && dx12ImGuiLibrary, "A critical resource is null");

		dx12::RLibraryImGui::PlatObj* platObj =
			dx12ImGuiLibrary->GetPlatformObject()->As<dx12::RLibraryImGui::PlatObj*>();

		SEEndCPUEvent(); // "RLibraryImGui::Execute: Setup"

		if (payload->m_perFrameCommands->HasCommandsToExecute(payload->m_currentFrameNum))
		{
			SEBeginCPUEvent("RLibraryImGui::Execute: Has commands");

			// Start the ImGui Frame:
			SEBeginCPUEvent("RLibraryImGui::Execute: Start ImGui frame");
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			SEEndCPUEvent(); // "RLibraryImGui::Execute: Start ImGui frame"

			// Execute our queued commands:
			//-----------------------------
			SEBeginCPUEvent("RLibraryImGui::Execute: Execute commands");
			payload->m_perFrameCommands->Execute(payload->m_currentFrameNum);
			SEEndCPUEvent(); // "RLibraryImGui::Execute: Execute commands"

			// ImGui internal rendering:
			SEBeginCPUEvent("RLibraryImGui::Execute: ImGui render");
			ImGui::Render(); // Note: Does not touch the GPU/graphics API
			SEEndCPUEvent(); // "RLibraryImGui::Execute: ImGui render"

			// Get our SE rendering objects:
			SEBeginCPUEvent("RLibraryImGui::Execute: Prepare command list");
			dx12::CommandList* commandList = static_cast<dx12::CommandList*>(platformObject);
			SEAssert(commandList && commandList->GetCommandListType() == dx12::CommandListType::Direct,
				"ImGui expects a graphics command list")

				Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> d3dCommandList = commandList->GetD3DCommandList();

#if defined(DEBUG_CMD_LIST_LOG_STAGE_NAMES)
			commandList->RecordStageName("<Library: ImGui>");
#endif

			SEBeginGPUEvent(d3dCommandList.Get(), perfmarkers::Type::GraphicsCommandList, "Render ImGui");

			ID3D12DescriptorHeap* descriptorHeap = platObj->m_imGuiGPUVisibleSRVDescriptorHeap.Get();
			d3dCommandList->SetDescriptorHeaps(1, &descriptorHeap);

			// Draw directly to the swapchain backbuffer
			re::SwapChain const& swapChain = context->GetSwapChain();
			commandList->SetRenderTargets(*dx12::SwapChain::GetBackBufferTargetSet(swapChain));

			SEEndCPUEvent(); // "RLibraryImGui::Execute: Prepare command list"

			// Record our ImGui draws:
			SEBeginCPUEvent("RLibraryImGui::Execute: Record ImGui draws");
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3dCommandList.Get());
			SEEndCPUEvent(); // "RLibraryImGui::Execute: Record ImGui draws"

			SEEndGPUEvent(d3dCommandList.Get());

			SEEndCPUEvent(); // "RLibraryImGui::Execute: Has commands"
		}

		// Descriptor deferred delete queue:
		platObj->FreeTempDescriptors(context->GetCurrentRenderFrameNum());

		SEEndCPUEvent(); // "RLibraryImGui::Execute"
	}
}
