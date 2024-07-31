// © 2024 Adam Badke. All rights reserved.
#include "Context.h"
#include "Context_DX12.h"
#include "ProfilingMarkers.h"
#include "RLibrary_ImGui_DX12.h"
#include "RenderManager.h"
#include "RenderStage.h"
#include "SysInfo_DX12.h"
#include "SwapChain_DX12.h"
#include "Texture_DX12.h"
#include "Window_Win32.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	std::unique_ptr<platform::RLibrary> RLibraryImGui::Create()
	{
		std::unique_ptr<platform::RLibrary> newLibrary = std::make_unique<dx12::RLibraryImGui>();

		platform::RLibraryImGui* imguiLibrary = dynamic_cast<platform::RLibraryImGui*>(newLibrary.get());
		platform::RLibraryImGui::CreateInternal(*imguiLibrary);

		dx12::RLibraryImGui::PlatformParams* platParams = 
			imguiLibrary->GetPlatformParams()->As<dx12::RLibraryImGui::PlatformParams*>();

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		ID3D12Device2* device = context->GetDevice().GetD3DDisplayDevice();
		re::SwapChain& swapChain = context->GetSwapChain();

		const uint8_t numFramesInFlight = re::RenderManager::Get()->GetNumFramesInFlight();

		// Imgui descriptor heap: Holds a single, CPU and GPU-visible SRV descriptor for the internal font texture
		const D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = 1,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = dx12::SysInfo::GetDeviceNodeMask() };

		HRESULT hr = device->CreateDescriptorHeap(
			&descriptorHeapDesc, IID_PPV_ARGS(&platParams->m_imGuiGPUVisibleSRVDescriptorHeap));
		CheckHResult(hr, "Failed to create single element descriptor heap for ImGui SRV");

		SEAssert(re::Context::Get()->GetWindow(), "Window pointer cannot be null");
		win32::Window::PlatformParams* windowPlatParams =
			re::Context::Get()->GetWindow()->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		dx12::Texture::PlatformParams const* backbufferColorTarget0PlatParams =
			dx12::SwapChain::GetBackBufferTargetSet(swapChain)->GetColorTarget(0).GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		// Setup ImGui platform/Renderer backends:
		ImGui_ImplWin32_Init(windowPlatParams->m_hWindow);
		ImGui_ImplDX12_Init(
			device,
			numFramesInFlight,
			backbufferColorTarget0PlatParams->m_format,
			platParams->m_imGuiGPUVisibleSRVDescriptorHeap.Get(),
			platParams->m_imGuiGPUVisibleSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			platParams->m_imGuiGPUVisibleSRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		platParams->m_imGuiGPUVisibleSRVDescriptorHeap->SetName(L"Imgui descriptor heap");

		return std::move(newLibrary);
	}


	void RLibraryImGui::Destroy()
	{
		LOG("Destroying ImGui render library");

		// ImGui Cleanup:
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}


	void RLibraryImGui::Execute(re::RenderStage* renderStage)
	{
		re::LibraryStage* imGuiStage = dynamic_cast<re::LibraryStage*>(renderStage);

		std::unique_ptr<re::LibraryStage::IPayload> iPayload = imGuiStage->TakePayload();
		platform::RLibraryImGui::Payload* payload = dynamic_cast<platform::RLibraryImGui::Payload*>(iPayload.get());

		RLibraryImGui* imGuiLibrary = dynamic_cast<RLibraryImGui*>(
			re::Context::Get()->GetOrCreateRenderLibrary(platform::RLibrary::ImGui));

		SEAssert(imGuiStage && payload && imGuiLibrary, "A critical resource is null");

		dx12::RLibraryImGui::PlatformParams* platParams = 
			imGuiLibrary->GetPlatformParams()->As<dx12::RLibraryImGui::PlatformParams*>();

		if (payload->m_perFrameCommands->HasCommandsToExecute(payload->m_currentFrameNum))
		{
			// Start the ImGui Frame:
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();


			// Execute our queued commands:
			payload->m_perFrameCommands->Execute(payload->m_currentFrameNum);


			// ImGui internal rendering
			ImGui::Render(); // Note: Does not touch the GPU/graphics API

			// Get our SE rendering objects:
			dx12::Context* context = re::Context::GetAs<dx12::Context*>();
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
	}
}
