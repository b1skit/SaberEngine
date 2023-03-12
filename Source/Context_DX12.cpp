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
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"
#include "Window_Win32.h"


namespace dx12
{
	using Microsoft::WRL::ComPtr;


	Context::PlatformParams::PlatformParams()
	{
		for (uint32_t i = 0; i < dx12::RenderManager::k_numFrames; i++)
		{
			m_frameFenceValues[i] = 0;
		}
	}


	void Context::Create(re::Context& context)
	{
		dx12::Context::PlatformParams* ctxPlatParams =
			context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		EnableDebugLayer();

		ctxPlatParams->m_device.Create();


		// Descriptor heap managers:
		ctxPlatParams->m_descriptorHeapMgrs.reserve(static_cast<size_t>(DescriptorHeapType_Count));
		for (size_t i = 0; i < DescriptorHeapType_Count; i++)
		{
			switch (static_cast<DescriptorHeapType>(i))
			{
			case DescriptorHeapType::CBV_SRV_UAV:
			{
				ctxPlatParams->m_descriptorHeapMgrs.emplace_back(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
			break;
			case DescriptorHeapType::Sampler:
			{
				ctxPlatParams->m_descriptorHeapMgrs.emplace_back(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			}
			break;
			case DescriptorHeapType::RTV:
			{
				ctxPlatParams->m_descriptorHeapMgrs.emplace_back(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			}
			break;
			case DescriptorHeapType::DSV:
			{
				ctxPlatParams->m_descriptorHeapMgrs.emplace_back(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
			}
			break;
			default:
				SEAssertF("Invalid descriptor heap type");
			}
		}


		// Command Queues:
		// TODO: Create/support more command queue types
		ID3D12Device2* device = ctxPlatParams->m_device.GetD3DDisplayDevice();

		ctxPlatParams->m_commandQueues[CommandList::CommandListType::Direct].Create(
			device, CommandList::CommandListType::Direct);

		ctxPlatParams->m_commandQueues[CommandList::CommandListType::Copy].Create(
			device, CommandList::CommandListType::Copy);


		// NOTE: Currently, this call retrieves m_commandQueue from the Context platform params
		// TODO: Clean this up, it's gross.
		context.GetSwapChain().Create();
		
		dx12::SwapChain::PlatformParams* swapChainParams = 
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		SEAssert("Window pointer cannot be null", en::CoreEngine::Get()->GetWindow());
		win32::Window::PlatformParams* windowPlatParams = 
			en::CoreEngine::Get()->GetWindow()->GetPlatformParams()->As<win32::Window::PlatformParams*>();

		dx12::TextureTargetSet::PlatformParams* swapChainTargetSetParams = 
			swapChainParams->m_backbufferTargetSets[0]->GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		// Setup our ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = re::k_imguiIniPath;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();



		// TODO: Create Imgui descriptor handles
		// - Needs a single CPU-visible and single GPU-visible SRV descriptor for the internal font texture
		// -> Some code also commented out in Context::Destroy
		
		// Setup Platform/Renderer backends
		ImGui_ImplWin32_Init(windowPlatParams->m_hWindow);
		//ImGui_ImplDX12_Init(
		//	ctxPlatParams->m_device.GetD3DDisplayDevice(),
		//	dx12::RenderManager::k_numFrames, // Number of frames in flight
		//	swapChainTargetSetParams->m_renderTargetFormats.RTFormats[0],
		//	ctxPlatParams->m_RTVDescHeap.Get(),
		//	ctxPlatParams->m_RTVDescHeap->GetCPUDescriptorHandleForHeapStart(),
		//	ctxPlatParams->m_RTVDescHeap->GetGPUDescriptorHandleForHeapStart());
	}


	void Context::Destroy(re::Context& context)
	{
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		if (!ctxPlatParams)
		{
			return;
		}

		// ImGui Cleanup:
		//ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();		

		// Make sure our command queues have finished all commands before closing.
		ctxPlatParams->m_commandQueues[CommandList::Copy].Flush();
		ctxPlatParams->m_commandQueues[CommandList::Copy].Destroy();
		
		ctxPlatParams->m_commandQueues[CommandList::Direct].Flush();
		ctxPlatParams->m_commandQueues[CommandList::Direct].Destroy();

		context.GetSwapChain().Destroy();

		ctxPlatParams->m_descriptorHeapMgrs.clear();

		ctxPlatParams->m_device.Destroy();
	}


	void Context::Present(re::Context const& context)
	{
		// Create a command list to transition the backbuffer to the presentation state
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		dx12::CommandQueue& directQueue = ctxPlatParams->m_commandQueues[dx12::CommandList::Direct];

		// Add a GPU wait to ensure our graphics work has finished before we present
		directQueue.GPUWait(ctxPlatParams->m_lastFenceBeforePresent);

		// Note: Our command lists and associated command allocators are already closed/reset
		std::shared_ptr<dx12::CommandList> commandList = directQueue.GetCreateCommandList();

		Microsoft::WRL::ComPtr<ID3D12Resource> backbufferResource =
			dx12::SwapChain::GetBackBufferResource(context.GetSwapChain());

		// Transition our backbuffer resource back to the present state:
		commandList->TransitionResource(
			backbufferResource.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT);

		std::shared_ptr<dx12::CommandList> commandLists[] =
		{
			commandList
		};

		directQueue.Execute(1, commandLists);

		// Present:
		dx12::SwapChain::PlatformParams* swapChainPlatParams = 
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		const uint8_t backbufferIdx = dx12::SwapChain::GetBackBufferIdx(context.GetSwapChain());

		// Present the backbuffer:
		const bool vsyncEnabled = swapChainPlatParams->m_vsyncEnabled;
		const uint32_t syncInterval = vsyncEnabled ? 1 : 0;
		const uint32_t presentFlags = 
			(swapChainPlatParams->m_tearingSupported && !vsyncEnabled) ? DXGI_PRESENT_ALLOW_TEARING : 0;

		swapChainPlatParams->m_swapChain->Present(syncInterval, presentFlags);

		// Insert a signal into the command queue:
		ctxPlatParams->m_frameFenceValues[backbufferIdx] = 
			ctxPlatParams->m_commandQueues[CommandList::Direct].Signal();
		// TODO: We should maintain a frame fence, and individual fences per command queue

		// Get the next backbuffer index:
		// Note: Backbuffer indices are not guaranteed to be sequential if we're using DXGI_SWAP_EFFECT_FLIP_DISCARD
		swapChainPlatParams->m_backBufferIdx = swapChainPlatParams->m_swapChain->GetCurrentBackBufferIndex();
		
		// Wait on the fence for the next backbuffer, to ensure its previous frame is done (blocking)
		ctxPlatParams->m_commandQueues[CommandList::Direct].WaitForGPU(ctxPlatParams->m_frameFenceValues[backbufferIdx]);

		// Free the descriptors used on the next backbuffer now that we know the fence has been signalled:
		for (size_t i = 0; i < DescriptorHeapType_Count; i++)
		{
			ctxPlatParams->m_descriptorHeapMgrs[static_cast<DescriptorHeapType>(i)].ReleaseFreedAllocations(
				ctxPlatParams->m_frameFenceValues[backbufferIdx]);
		}	
	}


	std::shared_ptr<dx12::PipelineState> Context::CreateAddPipelineState(
		gr::PipelineState const& grPipelineState, 
		re::Shader const& shader, 
		D3D12_RT_FORMAT_ARRAY const& rtvFormats, 
		const DXGI_FORMAT dsvFormat)
	{
		dx12::Context::PlatformParams* ctxPlatParams = 
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		// TEMP HAX: For now, we just have a single PSO, so just hard-code it. TODO: Create a library of pre-computed
		// PSOs at startup
		ctxPlatParams->m_pipelineState = std::make_shared<dx12::PipelineState>(
			grPipelineState,
			&shader,
			rtvFormats, 
			dsvFormat);

		LOG_ERROR("TODO: Implement dx12::Context::CreateAddPipelineState correctly");

		return ctxPlatParams->m_pipelineState;
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


	CommandQueue& GetCommandQueue(CommandList::CommandListType type)
	{
		dx12::Context::PlatformParams* ctxPlatParams = 
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		return ctxPlatParams->m_commandQueues[type];
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
}