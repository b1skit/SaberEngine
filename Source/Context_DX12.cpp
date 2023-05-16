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
		ctxPlatParams->m_cpuDescriptorHeapMgrs.reserve(static_cast<size_t>(CPUDescriptorHeapType_Count));
		for (size_t i = 0; i < CPUDescriptorHeapType_Count; i++)
		{
			switch (static_cast<CPUDescriptorHeapType>(i))
			{
			case CPUDescriptorHeapType::CBV_SRV_UAV:
			{
				ctxPlatParams->m_cpuDescriptorHeapMgrs.emplace_back(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
			break;
			case CPUDescriptorHeapType::Sampler:
			{
				ctxPlatParams->m_cpuDescriptorHeapMgrs.emplace_back(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
			}
			break;
			case CPUDescriptorHeapType::RTV:
			{
				ctxPlatParams->m_cpuDescriptorHeapMgrs.emplace_back(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			}
			break;
			case CPUDescriptorHeapType::DSV:
			{
				ctxPlatParams->m_cpuDescriptorHeapMgrs.emplace_back(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
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
			swapChainParams->m_backbufferTargetSet->GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		// Setup our ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = re::k_imguiIniPath;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Imgui descriptor heap: Holds a single, CPU and GPU-visible SRV descriptor for the internal font texture
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
		descriptorHeapDesc.Type				= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptorHeapDesc.NumDescriptors	= 1;
		descriptorHeapDesc.Flags			= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		descriptorHeapDesc.NodeMask			= deviceNodeMask;

		HRESULT hr = device->CreateDescriptorHeap(
			&descriptorHeapDesc, IID_PPV_ARGS(&ctxPlatParams->m_imGuiGPUVisibleSRVDescriptorHeap));
		CheckHResult(hr, "Failed to create single element descriptor heap for ImGui SRV");

		// Setup ImGui platform/Renderer backends:
		ImGui_ImplWin32_Init(windowPlatParams->m_hWindow);
		ImGui_ImplDX12_Init(
			ctxPlatParams->m_device.GetD3DDisplayDevice(),
			dx12::RenderManager::k_numFrames, // Number of frames in flight
			swapChainTargetSetParams->m_renderTargetFormats.RTFormats[0],
			ctxPlatParams->m_imGuiGPUVisibleSRVDescriptorHeap.Get(),
			ctxPlatParams->m_imGuiGPUVisibleSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			ctxPlatParams->m_imGuiGPUVisibleSRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	}


	void Context::Destroy(re::Context& context)
	{
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		if (!ctxPlatParams)
		{
			return;
		}

		// ImGui Cleanup:
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();		

		// Make sure our command queues have finished all commands before closing.
		ctxPlatParams->m_commandQueues[CommandList::Copy].Flush();
		ctxPlatParams->m_commandQueues[CommandList::Copy].Destroy();
		
		ctxPlatParams->m_commandQueues[CommandList::Direct].Flush();
		ctxPlatParams->m_commandQueues[CommandList::Direct].Destroy();

		context.GetSwapChain().Destroy();

		// NOTE: We must destroy anything that holds a parameter block before the ParameterBlockAllocator is destroyed, 
		// as parameter blocks call the ParameterBlockAllocator in their destructor
		context.GetParameterBlockAllocator().Destroy();

		// DX12 parameter blocks contain cpu descriptors, so we must destroy the cpu descriptor heap manager after the
		// parameter block allocator
		ctxPlatParams->m_cpuDescriptorHeapMgrs.clear();

		ctxPlatParams->m_device.Destroy();
	}


	void Context::Present(re::Context const& context)
	{
		// Create a command list to transition the backbuffer to the presentation state
		dx12::Context::PlatformParams* ctxPlatParams = context.GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		dx12::CommandQueue& directQueue = ctxPlatParams->m_commandQueues[dx12::CommandList::Direct];

		std::shared_ptr<dx12::CommandList> commandList = directQueue.GetCreateCommandList();

		Microsoft::WRL::ComPtr<ID3D12Resource> backbufferResource =
			dx12::SwapChain::GetBackBufferResource(context.GetSwapChain());

		// Transition our backbuffer resource back to the present state:
		commandList->TransitionResource(
			backbufferResource.Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		std::shared_ptr<dx12::CommandList> commandLists[] =
		{
			commandList
		};
		directQueue.Execute(1, commandLists);

		// Present:
		dx12::SwapChain::PlatformParams* swapChainPlatParams = 
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		const uint8_t currentFrameBackbufferIdx = dx12::SwapChain::GetBackBufferIdx(context.GetSwapChain());

		// Present the backbuffer:
		const bool vsyncEnabled = swapChainPlatParams->m_vsyncEnabled;
		const uint32_t syncInterval = vsyncEnabled ? 1 : 0;
		const uint32_t presentFlags = 
			(swapChainPlatParams->m_tearingSupported && !vsyncEnabled) ? DXGI_PRESENT_ALLOW_TEARING : 0;

		HRESULT hr = swapChainPlatParams->m_swapChain->Present(syncInterval, presentFlags);
		if (hr == DXGI_STATUS_OCCLUDED)
		{
			// TODO: Handle this.
			// The window content is not visible. When receiving this status, an application can stop rendering and use
			// DXGI_PRESENT_TEST to determine when to resume rendering. You will not receive DXGI_STATUS_OCCLUDED if
			// you're using a flip model swap chain.
		}
		else
		{
			CheckHResult(hr, "Failed to present");
		}

		// Insert a signal into the command queue: Once this is reached, we know the work for the current frame is done
		ctxPlatParams->m_frameFenceValues[currentFrameBackbufferIdx] = 
			ctxPlatParams->m_commandQueues[CommandList::Direct].GPUSignal();

		// Get the next backbuffer index (Note: Backbuffer indices are not guaranteed to be sequential if we're using 
		// DXGI_SWAP_EFFECT_FLIP_DISCARD)
		const uint8_t nextFrameBackbufferIdx = swapChainPlatParams->m_swapChain->GetCurrentBackBufferIndex();

		swapChainPlatParams->m_backBufferIdx = nextFrameBackbufferIdx;
		
		// Block the CPU on the fence for our new backbuffer, to ensure all of its work is done
		ctxPlatParams->m_commandQueues[CommandList::Direct].CPUWait(
			ctxPlatParams->m_frameFenceValues[nextFrameBackbufferIdx]);

		// Free the descriptors used on the next backbuffer now that we know the fence has been reached:
		for (size_t i = 0; i < CPUDescriptorHeapType_Count; i++)
		{
			ctxPlatParams->m_cpuDescriptorHeapMgrs[static_cast<CPUDescriptorHeapType>(i)].ReleaseFreedAllocations(
				ctxPlatParams->m_frameFenceValues[nextFrameBackbufferIdx]);
		}	
	}


	void Context::CreateAddPipelineState(
		re::Shader const& shader,
		gr::PipelineState& grPipelineState,	
		re::TextureTargetSet& targetSet)
	{
		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		// TODO: We should combine all the keys into a single value instead of nesting hash tables
		const uint64_t shaderKey = shader.GetNameID();
		const uint64_t pipelineKey = grPipelineState.GetPipelineStateDataHash();
		const uint64_t targetSetKey = targetSet.GetTargetSetSignature();

		SEAssert("PSO already exists! This is not a bug: This assert is to validate the system works. If you hit this, "
			"delete the assert and give yourself a high-five",
			!ctxPlatParams->m_PSOLibrary.contains(shaderKey) ||
			!ctxPlatParams->m_PSOLibrary[shaderKey].contains(pipelineKey) ||
			!ctxPlatParams->m_PSOLibrary[shaderKey][pipelineKey].contains(targetSetKey));

		std::shared_ptr<dx12::PipelineState> newPSO = std::make_shared<dx12::PipelineState>();
		newPSO->Create(shader, grPipelineState, targetSet);
		
		ctxPlatParams->m_PSOLibrary[shaderKey][pipelineKey][targetSetKey] = newPSO;
	}


	uint8_t Context::GetMaxTextureInputs() // TODO: This should be a member of SysInfo
	{
		SEAssertF("TODO: Implement this"); 
		return 0;
	}


	uint8_t Context::GetMaxColorTargets() // TODO: This should be a member of SysInfo
	{
		return D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	}


	CommandQueue& Context::GetCommandQueue(CommandList::CommandListType type)
	{
		dx12::Context::PlatformParams* ctxPlatParams = 
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		return ctxPlatParams->m_commandQueues[type];
	}


	dx12::GlobalResourceStateTracker& Context::GetGlobalResourceStateTracker()
	{
		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		return ctxPlatParams->m_globalResourceStates;
	}


	std::shared_ptr<dx12::PipelineState> Context::GetPipelineStateObject(
		re::Shader const& shader,
		gr::PipelineState& grPipelineState,
		re::TextureTargetSet* targetSet)
	{
		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
		
		const uint64_t shaderKey = shader.GetNameID();
		const uint64_t pipelineKey = grPipelineState.GetPipelineStateDataHash();
		const uint64_t targetSetKey = targetSet ? targetSet->GetTargetSetSignature() : 0;

		SEAssert("Could not find matching PSO", 
			ctxPlatParams->m_PSOLibrary.contains(shaderKey) && 
			ctxPlatParams->m_PSOLibrary[shaderKey].contains(pipelineKey) &&
			ctxPlatParams->m_PSOLibrary[shaderKey][pipelineKey].contains(targetSetKey));

		return ctxPlatParams->m_PSOLibrary[shaderKey][pipelineKey][targetSetKey];
	}
}