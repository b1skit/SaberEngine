// © 2024 Adam Badke. All rights reserved.
#include "Context.h"
#include "Context_DX12.h"
#include "RLibrary_ImGui_DX12.h"
#include "RenderManager.h"
#include "RenderStage.h"
#include "SysInfo_DX12.h"
#include "SwapChain_DX12.h"
#include "Texture_DX12.h"
#include "Window_Win32.h"

#include "Core/ProfilingMarkers.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	void RLibraryImGui::PlatformParams::InitializeImGuiSRVHeap()
	{
		util::ScopedThreadProtector scopedThreadProtector(m_threadProtector);

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

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
	}


	void RLibraryImGui::PlatformParams::DestroyImGuiSRVHeap()
	{
		util::ScopedThreadProtector scopedThreadProtector(m_threadProtector);

		SEAssert(m_freeIndices.size() == k_imguiHeapSize, "Missing ImGui free indices - have all been returned?");

		m_imGuiGPUVisibleSRVDescriptorHeap = nullptr;
		m_heapStartCPU = {};
		m_heapStartGPU = {};
		m_handleIncrementSize = 0;
		m_freeIndices.clear();
	}


	void RLibraryImGui::PlatformParams::Allocate(
		ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandleOut, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandleOut)
	{
		dx12::RLibraryImGui* dx12ImGuiLibrary = nullptr;
		if (info) // Might be null if we're calling internally
		{
			dx12ImGuiLibrary = static_cast<dx12::RLibraryImGui*>(info->UserData);
		}
		else
		{
			dx12ImGuiLibrary = dynamic_cast<dx12::RLibraryImGui*>(
				re::Context::Get()->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));
		}
		SEAssert(dx12ImGuiLibrary, "Failed to get RLibraryImGui");

		dx12::RLibraryImGui::PlatformParams* platParams =
			dx12ImGuiLibrary->GetPlatformParams()->As<dx12::RLibraryImGui::PlatformParams*>();

		util::ScopedThreadProtector scopedThreadProtector(platParams->m_threadProtector);

		SEAssert(!platParams->m_freeIndices.empty(),
			"No free indices to allocate from. Consider increasing k_imguiHeapSize");

		const uint32_t allocationIdx = platParams->m_freeIndices.back();
		platParams->m_freeIndices.pop_back();

		cpuHandleOut->ptr = platParams->m_heapStartCPU.ptr + (allocationIdx * platParams->m_handleIncrementSize);
		gpuHandleOut->ptr = platParams->m_heapStartGPU.ptr + (allocationIdx * platParams->m_handleIncrementSize);
	}


	void RLibraryImGui::PlatformParams::Free(
		ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHanle)
	{
		dx12::RLibraryImGui* dx12ImGuiLibrary = nullptr;
		if (info) // Might be null if we're calling internally
		{
			dx12ImGuiLibrary = static_cast<dx12::RLibraryImGui*>(info->UserData);
		}
		else
		{
			dx12ImGuiLibrary = dynamic_cast<dx12::RLibraryImGui*>(
				re::Context::Get()->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));
		}
		SEAssert(dx12ImGuiLibrary, "Failed to get RLibraryImGui");

		dx12::RLibraryImGui::PlatformParams* platParams =
			dx12ImGuiLibrary->GetPlatformParams()->As<dx12::RLibraryImGui::PlatformParams*>();

		util::ScopedThreadProtector scopedThreadProtector(platParams->m_threadProtector);

		const uint32_t cpuIdx = 
			static_cast<uint32_t>((cpuHandle.ptr - platParams->m_heapStartCPU.ptr) / platParams->m_handleIncrementSize);

		SEAssert(cpuIdx == 
			static_cast<uint32_t>((gpuHanle.ptr - platParams->m_heapStartGPU.ptr) / platParams->m_handleIncrementSize),
			"CPU and GPU heap pointers are out of sync");
		
		platParams->m_freeIndices.push_back(cpuIdx);
	}


	void RLibraryImGui::PlatformParams::CopyTempDescriptorToImGuiHeap(
		D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor,
		D3D12_CPU_DESCRIPTOR_HANDLE& cpuDstOut,
		D3D12_GPU_DESCRIPTOR_HANDLE& gpuDstOut)
	{
		// Allocate a destination in our ImGui descriptor heap:
		Allocate(nullptr, &cpuDstOut, &gpuDstOut);

		util::ScopedThreadProtector scopedThreadProtector(m_threadProtector); // Avoid recursive call in Allocate()

		// Copy the descriptor in:
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

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
			re::RenderManager::Get()->GetCurrentRenderFrameNum(),
			TempDescriptorAllocation{ 
				cpuDstOut, 
				gpuDstOut });
	}


	void RLibraryImGui::PlatformParams::FreeTempDescriptors(uint64_t currentFrame)
	{
		{
			util::ScopedThreadProtector scopedThreadProtector(m_threadProtector); // Avoid recursive call in Free()

			if (m_deferredDescriptorDelete.empty())
			{
				return;
			}
		}

		// Defer deletion by numFramesInFlight
		const uint8_t numFramesInFlight = re::RenderManager::Get()->GetNumFramesInFlight();

		while (!m_deferredDescriptorDelete.empty() &&
			m_deferredDescriptorDelete.front().first + numFramesInFlight < currentFrame)
		{
			RLibraryImGui::PlatformParams::Free(
				nullptr, 
				m_deferredDescriptorDelete.front().second.m_cpuDesc, 
				m_deferredDescriptorDelete.front().second.m_gpuDesc);

			m_deferredDescriptorDelete.pop();
		}
	}


	std::unique_ptr<platform::RLibrary> RLibraryImGui::Create()
	{
		std::unique_ptr<platform::RLibrary> newLibrary = std::make_unique<dx12::RLibraryImGui>();

		dx12::RLibraryImGui* dx12ImGuiLibrary = dynamic_cast<dx12::RLibraryImGui*>(newLibrary.get());
		platform::RLibraryImGui::CreateInternal(*dx12ImGuiLibrary);

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();

		re::SwapChain& swapChain = context->GetSwapChain();

		// Setup ImGui platform/Renderer backends:
		SEAssert(re::Context::Get()->GetWindow(), "Window pointer cannot be null");
		win32::Window::PlatformParams* windowPlatParams =
			re::Context::Get()->GetWindow()->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		dx12::Texture::PlatformParams const* backbufferColorTarget0PlatParams =
			dx12::SwapChain::GetBackBufferTargetSet(swapChain)->GetColorTarget(0).GetTexture()
				->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		ImGui_ImplWin32_Init(windowPlatParams->m_hWindow);

		// Initialize our ImGui descriptor heap (lives in our PlatformParams):
		dx12::RLibraryImGui::PlatformParams* platParams = 
			dx12ImGuiLibrary->GetPlatformParams()->As<dx12::RLibraryImGui::PlatformParams*>();
		
		platParams->InitializeImGuiSRVHeap();

		// ImGui DX12 backend initialization:
		dx12::CommandQueue& directQueue = context->GetCommandQueue(dx12::CommandListType::Direct);

		const uint8_t numFramesInFlight = re::RenderManager::Get()->GetNumFramesInFlight();

		ImGui_ImplDX12_InitInfo initInfo{};
		initInfo.Device = device;
		initInfo.CommandQueue = directQueue.GetD3DCommandQueue();
		initInfo.NumFramesInFlight = numFramesInFlight;
		initInfo.RTVFormat = backbufferColorTarget0PlatParams->m_format;
		
		initInfo.SrvDescriptorHeap = platParams->m_imGuiGPUVisibleSRVDescriptorHeap.Get();
		initInfo.SrvDescriptorAllocFn = RLibraryImGui::PlatformParams::Allocate;
		initInfo.SrvDescriptorFreeFn = RLibraryImGui::PlatformParams::Free;

		// Store our RLibraryImGui pointer so we can access it during alloc/dealloc
		// Note: This is also required as otherwise the alloc/dealloc functions would recursively call this function
		initInfo.UserData = dx12ImGuiLibrary; 

		ImGui_ImplDX12_Init(&initInfo);

		return std::move(newLibrary);
	}


	void RLibraryImGui::CopyTempDescriptorToImGuiHeap(
		D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor,
		D3D12_CPU_DESCRIPTOR_HANDLE& cpuDstOut,
		D3D12_GPU_DESCRIPTOR_HANDLE& gpuDstOut)
	{
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		RLibraryImGui* dx12ImGuiLibrary = dynamic_cast<RLibraryImGui*>(
			context->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));

		dx12::RLibraryImGui::PlatformParams* platParams =
			dx12ImGuiLibrary->GetPlatformParams()->As<dx12::RLibraryImGui::PlatformParams*>();

		platParams->CopyTempDescriptorToImGuiHeap(srcDescriptor, cpuDstOut, gpuDstOut);
	}


	void RLibraryImGui::Destroy()
	{
		LOG("Destroying ImGui render library");

		// ImGui Cleanup:
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		// Clean up our ImGui descriptor heap:
		RLibraryImGui* dx12ImGuiLibrary = dynamic_cast<RLibraryImGui*>(
			re::Context::Get()->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));

		dx12::RLibraryImGui::PlatformParams* platParams =
			dx12ImGuiLibrary->GetPlatformParams()->As<dx12::RLibraryImGui::PlatformParams*>();

		platParams->FreeTempDescriptors(std::numeric_limits<uint64_t>::max());

		platParams->DestroyImGuiSRVHeap();
	}


	void RLibraryImGui::Execute(re::RenderStage* renderStage)
	{
		re::LibraryStage* imGuiStage = dynamic_cast<re::LibraryStage*>(renderStage);

		std::unique_ptr<re::LibraryStage::IPayload> iPayload = imGuiStage->TakePayload();
		platform::RLibraryImGui::Payload* payload = dynamic_cast<platform::RLibraryImGui::Payload*>(iPayload.get());

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		RLibraryImGui* dx12ImGuiLibrary = dynamic_cast<RLibraryImGui*>(
			context->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));

		SEAssert(imGuiStage && payload && dx12ImGuiLibrary, "A critical resource is null");

		dx12::RLibraryImGui::PlatformParams* platParams =
			dx12ImGuiLibrary->GetPlatformParams()->As<dx12::RLibraryImGui::PlatformParams*>();

		if (payload->m_perFrameCommands->HasCommandsToExecute(payload->m_currentFrameNum))
		{
			// Start the ImGui Frame:
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// Execute our queued commands:
			//-----------------------------
			payload->m_perFrameCommands->Execute(payload->m_currentFrameNum);

			// ImGui internal rendering:
			ImGui::Render(); // Note: Does not touch the GPU/graphics API

			// Get our SE rendering objects:
			dx12::CommandQueue& directQueue = context->GetCommandQueue(dx12::CommandListType::Direct);

			// Configure the command list:
			std::shared_ptr<dx12::CommandList> commandList = directQueue.GetCreateCommandList();
			ID3D12GraphicsCommandList2* d3dCommandList = commandList->GetD3DCommandList();

#if defined(DEBUG_CMD_LIST_LOG_STAGE_NAMES)
			commandList->RecordStageName("<Library: ImGui>");
#endif

			SEBeginGPUEvent(d3dCommandList, perfmarkers::Type::GraphicsCommandList, "Render ImGui");

			ID3D12DescriptorHeap* descriptorHeap = platParams->m_imGuiGPUVisibleSRVDescriptorHeap.Get();
			d3dCommandList->SetDescriptorHeaps(1, &descriptorHeap);

			// Draw directly to the swapchain backbuffer
			re::SwapChain const& swapChain = context->GetSwapChain();
			commandList->SetRenderTargets(*dx12::SwapChain::GetBackBufferTargetSet(swapChain));

			// Record our ImGui draws:
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3dCommandList);

			SEEndGPUEvent(d3dCommandList);

			// Submit the populated command list:
			directQueue.Execute(1, &commandList);
		}

		// Descriptor deferred delete queue:
		platParams->FreeTempDescriptors(re::RenderManager::Get()->GetCurrentRenderFrameNum());
	}
}
