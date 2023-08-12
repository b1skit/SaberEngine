// © 2022 Adam Badke. All rights reserved.
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "CoreEngine.h"
#include "Debug_DX12.h"
#include "DebugConfiguration.h"
#include "Fence_DX12.h"
#include "HashUtils.h"
#include "RenderManager_DX12.h"
#include "SwapChain_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"
#include "Window_Win32.h"

using Microsoft::WRL::ComPtr;


namespace
{
	uint64_t ComputePSOKey(
		re::Shader const& shader, 
		gr::PipelineState const& grPipelineState, 
		re::TextureTargetSet const* targetSet)
	{
		const uint64_t shaderKey = shader.GetNameID();
		const uint64_t pipelineKey = grPipelineState.GetPipelineStateDataHash();
		const uint64_t targetSetKey = targetSet ? targetSet->GetTargetSetSignature() : 0;

		uint64_t psoKey = 0;
		util::CombineHash(psoKey, shaderKey);
		util::CombineHash(psoKey, pipelineKey);
		util::CombineHash(psoKey, targetSetKey);
		return psoKey;
	}
}


namespace dx12
{
	void Context::Create()
	{
		memset(m_frameFenceValues, 0, sizeof(uint64_t) * dx12::RenderManager::GetNumFrames());

		EnableDebugLayer(); // Before we create a device

		m_device.Create();

		// Descriptor heap managers:
		m_cpuDescriptorHeapMgrs.reserve(static_cast<size_t>(CPUDescriptorHeapManager::HeapType_Count));

		m_cpuDescriptorHeapMgrs.emplace_back(CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV);
		m_cpuDescriptorHeapMgrs.emplace_back(CPUDescriptorHeapManager::HeapType::RTV);
		m_cpuDescriptorHeapMgrs.emplace_back(CPUDescriptorHeapManager::HeapType::DSV);

		// Command Queues:
		ID3D12Device2* device = m_device.GetD3DDisplayDevice();

		m_commandQueues[CommandListType::Direct].Create(device, CommandListType::Direct);
		m_commandQueues[CommandListType::Compute].Create(device, CommandListType::Compute);
		m_commandQueues[CommandListType::Copy].Create(device, CommandListType::Copy);

		// NOTE: Must create the swapchain after our command queues. This is because the DX12 swapchain creation
		// requires a direct command queue; dx12::SwapChain::Create recursively gets it from the Context platform params
		re::SwapChain& swapChain = GetSwapChain();
		swapChain.Create(); 
		
		// Setup our ImGui context
		{
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.IniFilename = re::k_imguiIniPath;

			// Setup Dear ImGui style
			ImGui::StyleColorsDark();

			// Imgui descriptor heap: Holds a single, CPU and GPU-visible SRV descriptor for the internal font texture
			constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

			D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
			descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			descriptorHeapDesc.NumDescriptors = 1;
			descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			descriptorHeapDesc.NodeMask = deviceNodeMask;

			HRESULT hr = device->CreateDescriptorHeap(
				&descriptorHeapDesc, IID_PPV_ARGS(&m_imGuiGPUVisibleSRVDescriptorHeap));
			CheckHResult(hr, "Failed to create single element descriptor heap for ImGui SRV");

			SEAssert("Window pointer cannot be null", en::CoreEngine::Get()->GetWindow());
			win32::Window::PlatformParams* windowPlatParams =
				en::CoreEngine::Get()->GetWindow()->GetPlatformParams()->As<win32::Window::PlatformParams*>();

			dx12::Texture::PlatformParams const* backbufferColorTarget0PlatParams =
				dx12::SwapChain::GetBackBufferTargetSet(swapChain)->GetColorTarget(0).GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

			// Setup ImGui platform/Renderer backends:
			ImGui_ImplWin32_Init(windowPlatParams->m_hWindow);
			ImGui_ImplDX12_Init(
				m_device.GetD3DDisplayDevice(),
				dx12::RenderManager::GetNumFrames(), // Number of frames in flight
				backbufferColorTarget0PlatParams->m_format,
				m_imGuiGPUVisibleSRVDescriptorHeap.Get(),
				m_imGuiGPUVisibleSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
				m_imGuiGPUVisibleSRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

			m_imGuiGPUVisibleSRVDescriptorHeap->SetName(L"Imgui descriptor heap");
		}
	}


	void Context::Destroy(re::Context& context)
	{
		dx12::Context& dx12Context = dynamic_cast<dx12::Context&>(context);

		// ImGui Cleanup:
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();		

		// Make sure our command queues have finished all commands before closing.
		dx12Context.m_commandQueues[dx12::CommandListType::Copy].Flush();
		dx12Context.m_commandQueues[dx12::CommandListType::Copy].Destroy();
		
		dx12Context.m_commandQueues[dx12::CommandListType::Direct].Flush();
		dx12Context.m_commandQueues[dx12::CommandListType::Direct].Destroy();

		dx12Context.GetSwapChain().Destroy();

		// NOTE: We must destroy anything that holds a parameter block before the ParameterBlockAllocator is destroyed, 
		// as parameter blocks call the ParameterBlockAllocator in their destructor
		dx12Context.GetParameterBlockAllocator().Destroy();

		// Clear the null descriptor libraries:
		{
			std::unique_lock<std::mutex> srvLock(dx12Context.s_nullSRVLibraryMutex);
			dx12Context.s_nullSRVLibrary.clear();
		}
		{
			std::unique_lock<std::mutex> srvLock(dx12Context.s_nullUAVLibraryMutex);
			dx12Context.s_nullUAVLibrary.clear();
		}

		// DX12 parameter blocks contain cpu descriptors, so we must destroy the cpu descriptor heap manager after the
		// parameter block allocator
		dx12Context.m_cpuDescriptorHeapMgrs.clear();

		dx12Context.m_PSOLibrary.clear();
		dx12Context.m_rootSigLibrary.clear();

		dx12Context.m_device.Destroy();
	}


	void Context::Present()
	{
		// Create a command list to transition the backbuffer to the presentation state
		dx12::CommandQueue& directQueue = m_commandQueues[dx12::CommandListType::Direct];

		std::shared_ptr<dx12::CommandList> commandList = directQueue.GetCreateCommandList();

		re::SwapChain const& swapChain = GetSwapChain();

		std::shared_ptr<re::TextureTargetSet> swapChainTargetSet = 
			SwapChain::GetBackBufferTargetSet(GetSwapChain());
		
		dx12::Texture::PlatformParams const* backbufferColorTexPlatParams =
			swapChainTargetSet->GetColorTarget(0).GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		// Transition our backbuffer resource back to the present state:
		commandList->TransitionResource(
			swapChainTargetSet->GetColorTarget(0).GetTexture(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		directQueue.Execute(1, &commandList);

		// Present:
		dx12::SwapChain::PlatformParams* swapChainPlatParams = 
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

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
		const uint8_t currentFrameBackbufferIdx = dx12::SwapChain::GetBackBufferIdx(swapChain);
		m_frameFenceValues[currentFrameBackbufferIdx] = m_commandQueues[dx12::CommandListType::Direct].GPUSignal();

		// Get the next backbuffer index (Note: Backbuffer indices are not guaranteed to be sequential if we're using 
		// DXGI_SWAP_EFFECT_FLIP_DISCARD)
		const uint8_t nextFrameBackbufferIdx = swapChainPlatParams->m_swapChain->GetCurrentBackBufferIndex();

		swapChainPlatParams->m_backBufferIdx = nextFrameBackbufferIdx;
		
		// Block the CPU on the fence for our new backbuffer, to ensure all of its work is done
		m_commandQueues[dx12::CommandListType::Direct].CPUWait(m_frameFenceValues[nextFrameBackbufferIdx]);

		// Free the descriptors used on the next backbuffer now that we know the fence has been reached:
		for (size_t i = 0; i < CPUDescriptorHeapManager::HeapType_Count; i++)
		{
			m_cpuDescriptorHeapMgrs[static_cast<CPUDescriptorHeapManager::HeapType>(i)].ReleaseFreedAllocations(
				m_frameFenceValues[nextFrameBackbufferIdx]);
		}	
	}


	std::shared_ptr<dx12::PipelineState> Context::CreateAddPipelineState(
		re::Shader const& shader,
		gr::PipelineState const& grPipelineState,	
		re::TextureTargetSet const& targetSet)
	{
		std::shared_ptr<dx12::PipelineState> pso = nullptr;

		const uint64_t psoKey = ComputePSOKey(shader, grPipelineState, &targetSet);
		if (m_PSOLibrary.contains(psoKey))
		{
			pso = m_PSOLibrary[psoKey];
		}
		else
		{
			pso = std::make_shared<dx12::PipelineState>();
			pso->Create(shader, grPipelineState, targetSet);

			m_PSOLibrary[psoKey] = pso;
		}
		return pso;
	}


	CommandQueue& Context::GetCommandQueue(dx12::CommandListType type)
	{
		return m_commandQueues[type];
	}


	dx12::CommandQueue& Context::GetCommandQueue(uint64_t fenceValue)
	{
		const dx12::CommandListType cmdListType = dx12::Fence::GetCommandListTypeFromFenceValue(fenceValue);
		return GetCommandQueue(cmdListType);
	}


	std::shared_ptr<dx12::PipelineState> Context::GetPipelineStateObject(
		re::Shader const& shader,
		gr::PipelineState const& grPipelineState,
		re::TextureTargetSet const* targetSet)
	{
		const uint64_t psoKey = ComputePSOKey(shader, grPipelineState, targetSet);
		if (m_PSOLibrary.contains(psoKey))
		{
			return m_PSOLibrary[psoKey];
		}
		else
		{
			LOG_WARNING("DX12 PSO for Shader \"%s\", TextureTargetSet \"%s\" does not exist and must be created "
				"immediately", shader.GetName().c_str(), targetSet->GetName().c_str());

			return CreateAddPipelineState(shader, grPipelineState, *targetSet);
		}
	}


	bool Context::HasRootSignature(uint64_t rootSigDescHash)
	{
		return m_rootSigLibrary.contains(rootSigDescHash);
	}


	std::shared_ptr<dx12::RootSignature> Context::GetRootSignature(uint64_t rootSigDescHash)
	{
		SEAssert("Root signature has not been added", HasRootSignature(rootSigDescHash));

		return m_rootSigLibrary[rootSigDescHash];
	}


	void Context::AddRootSignature(std::shared_ptr<dx12::RootSignature> rootSig)
	{
		auto result = m_rootSigLibrary.insert({ rootSig->GetRootSigDescHash(), rootSig });
		SEAssert("Root signature has already been added", result.second);
	}


	DescriptorAllocation const& Context::GetNullSRVDescriptor(D3D12_SRV_DIMENSION dimension, DXGI_FORMAT format)
	{
		std::unique_lock<std::mutex> lock(s_nullSRVLibraryMutex);

		auto dimensionResult = s_nullSRVLibrary.find(dimension);
		if (dimensionResult == s_nullSRVLibrary.end())
		{
			dimensionResult = 
				s_nullSRVLibrary.emplace(dimension, std::unordered_map<DXGI_FORMAT, DescriptorAllocation>()).first;
		}

		auto formatResult = dimensionResult->second.find(format);
		if (formatResult == dimensionResult->second.end())
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Format = format;
			srvDesc.ViewDimension = dimension;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

			switch (dimension)
			{
			case D3D12_SRV_DIMENSION_UNKNOWN:
			case D3D12_SRV_DIMENSION_BUFFER:
			case D3D12_SRV_DIMENSION_TEXTURE1D:
			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
			{
				SEAssertF("TODO: Handle this type");
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURE2D:
			{
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.MipLevels = 1;
				srvDesc.Texture2D.PlaneSlice = 0;
				srvDesc.Texture2D.ResourceMinLODClamp = 0;
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
			case D3D12_SRV_DIMENSION_TEXTURE2DMS:
			case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
			case D3D12_SRV_DIMENSION_TEXTURE3D:
			case D3D12_SRV_DIMENSION_TEXTURECUBE:
			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
			case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
			{
				SEAssertF("TODO: Handle this type");
			}
			break;
			default:
				SEAssertF("Invalid dimension");
			}


			DescriptorAllocation descriptor =
				std::move(m_cpuDescriptorHeapMgrs[CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV].Allocate(1));

			ID3D12Device2* device = m_device.GetD3DDisplayDevice();

			device->CreateShaderResourceView(
				nullptr,
				&srvDesc,
				descriptor.GetBaseDescriptor());

			formatResult = dimensionResult->second.emplace(format, std::move(descriptor)).first;
		}

		return formatResult->second;
	}


	DescriptorAllocation const& Context::GetNullUAVDescriptor(D3D12_UAV_DIMENSION dimension, DXGI_FORMAT format)
	{
		std::unique_lock<std::mutex> lock(s_nullUAVLibraryMutex);

		auto dimensionResult = s_nullUAVLibrary.find(dimension);
		if (dimensionResult == s_nullUAVLibrary.end())
		{
			dimensionResult = 
				s_nullUAVLibrary.emplace(dimension, std::unordered_map<DXGI_FORMAT, DescriptorAllocation>()).first;
		}

		auto formatResult = dimensionResult->second.find(format);
		if (formatResult == dimensionResult->second.end())
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = format;
			uavDesc.ViewDimension = dimension;

			switch (dimension)
			{
			case D3D12_UAV_DIMENSION_UNKNOWN:
			case D3D12_UAV_DIMENSION_BUFFER:
			case D3D12_UAV_DIMENSION_TEXTURE1D:
			case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
			{
				SEAssertF("TODO: Handle this type");
			}
			break;
			case D3D12_UAV_DIMENSION_TEXTURE2D:
			{
				uavDesc.Texture2D.MipSlice = 0;
				uavDesc.Texture2D.PlaneSlice = 0;
			}
			break;
			case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
			case D3D12_UAV_DIMENSION_TEXTURE2DMS:
			case D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY:
			case D3D12_UAV_DIMENSION_TEXTURE3D:
			{
				SEAssertF("TODO: Handle this type");
			}
			break;
			default:
				SEAssertF("Invalid dimension");
			}


			DescriptorAllocation descriptor =
				std::move(m_cpuDescriptorHeapMgrs[CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV].Allocate(1));

			ID3D12Device2* device = m_device.GetD3DDisplayDevice();

			device->CreateUnorderedAccessView(
				nullptr,
				nullptr,
				&uavDesc,
				descriptor.GetBaseDescriptor());

			formatResult = dimensionResult->second.emplace(format, std::move(descriptor)).first;
		}

		return formatResult->second;
	}
}