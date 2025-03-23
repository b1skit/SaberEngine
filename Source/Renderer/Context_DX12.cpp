// © 2022 Adam Badke. All rights reserved.
#include "Context_DX12.h"
#include "Debug_DX12.h"
#include "GPUTimer_DX12.h"
#include "PipelineState_DX12.h"
#include "RenderManager_DX12.h"
#include "Shader.h"
#include "SwapChain_DX12.h"
#include "SysInfo_DX12.h"
#include "TextureTarget_DX12.h"
#include "Texture_DX12.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/ProfilingMarkers.h"

using Microsoft::WRL::ComPtr;


// Set the DX12 Agility SDK parameters:
// https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/#2-set-agility-sdk-parameters
// Note: The D3D12SDKVersion can be found on the SDK downloads page: https://devblogs.microsoft.com/directx/directx12agility/
// Update the installed package via Project-> Manage NuGet Packages
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 611; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\"; }


namespace
{
	uint64_t ComputePSOKey(
		re::Shader const& shader,
		re::TextureTargetSet const* targetSet)
	{
		uint64_t psoKey = shader.GetShaderIdentifier();

		re::RasterizationState const* rasterizationState = shader.GetRasterizationState();

		SEAssert(shader.GetPipelineType() != re::Shader::PipelineType::Rasterization ||
			(rasterizationState && targetSet),
			"Rasterization shader does not have a pipeline state or target set. This is unexpected");

		if (rasterizationState)
		{
			util::CombineHash(psoKey, rasterizationState->GetDataHash());
			
			// We must consider the target set, as we must specify the RTV/DSV formats when creating a rasterization
			// pipeline state stream
			util::CombineHash(psoKey, targetSet->GetTargetSetSignature());
		}	
		
		return psoKey;
	}
}


namespace dx12
{
	Context::Context()
		: m_pixGPUCaptureModule(nullptr)
		, m_pixCPUCaptureModule(nullptr)
	{
	}


	void Context::CreateInternal(uint64_t currentFrame)
	{
		// PIX must be loaded before loading any D3D12 APIs
		const bool enablePIXPGPUrogrammaticCaptures = 
			core::Config::Get()->KeyExists(core::configkeys::k_pixGPUProgrammaticCapturesCmdLineArg);
		const bool enablePIXPCPUProgrammaticCaptures =
			core::Config::Get()->KeyExists(core::configkeys::k_pixCPUProgrammaticCapturesCmdLineArg);

		if (enablePIXPGPUrogrammaticCaptures && enablePIXPCPUProgrammaticCaptures)
		{
			LOG_ERROR("Cannot have PIX CPU and GPU captures enabled at the same time. Default is GPU capture, CPU "
				"capturing ignored");
		}

		if (enablePIXPGPUrogrammaticCaptures)
		{
			LOG("Loading DX12 PIX GPU programmatic capture module");
			m_pixGPUCaptureModule = PIXLoadLatestWinPixGpuCapturerLibrary(); // This must be done before loading any D3D12 APIs

			if (m_pixGPUCaptureModule == nullptr)
			{
				const HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
				CheckHResult(hr, "Failed to create PIX GPU capture module");
			}
		}
		else if (enablePIXPCPUProgrammaticCaptures)
		{
			LOG("Loading DX12 PIX CPU programmatic capture module");
			m_pixCPUCaptureModule = PIXLoadLatestWinPixTimingCapturerLibrary();

			if (m_pixCPUCaptureModule == nullptr)
			{
				const HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
				CheckHResult(hr, "Failed to create PIX GPU capture module");
			}
		}

		const uint8_t numFramesInFlight = dx12::RenderManager::GetNumFramesInFlight();

		m_frameFenceValues.resize(numFramesInFlight, 0);

		EnableDebugLayer(); // Before we create a device

		m_device.Create();

		LOG(std::format("D3D resource binding tier: {}",
			dx12::D3D12ResourceBindingTierToCStr(dx12::SysInfo::GetResourceBindingTier())).c_str());

		LOG(std::format("D3D heap tier: {}",
			dx12::D3D12ResourceHeapTierToCStr(dx12::SysInfo::GetResourceHeapTier())).c_str());

		// Descriptor heap managers:
		m_cpuDescriptorHeapMgrs.reserve(static_cast<size_t>(CPUDescriptorHeapManager::HeapType_Count));

		m_cpuDescriptorHeapMgrs.emplace_back(CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV);
		m_cpuDescriptorHeapMgrs.emplace_back(CPUDescriptorHeapManager::HeapType::RTV);
		m_cpuDescriptorHeapMgrs.emplace_back(CPUDescriptorHeapManager::HeapType::DSV);

		// Command Queues:
		Microsoft::WRL::ComPtr<ID3D12Device> device = m_device.GetD3DDevice();

		m_commandQueues[CommandListType::Direct].Create(device, CommandListType::Direct);
		m_commandQueues[CommandListType::Compute].Create(device, CommandListType::Compute);
		m_commandQueues[CommandListType::Copy].Create(device, CommandListType::Copy);

		m_heapManager.Initialize();

		// NOTE: Must create the swapchain after our command queues. This is because the DX12 swapchain creation
		// requires a direct command queue; dx12::SwapChain::Create recursively gets it from the Context platform params
		re::SwapChain& swapChain = GetSwapChain();
		swapChain.Create();

		// Buffer Allocator:
		m_bufferAllocator = re::BufferAllocator::Create();
		m_bufferAllocator->Initialize(currentFrame);
	}


	void Context::UpdateInternal(uint64_t currentFrame)
	{
		// Update the bindless resource manager.
		// Note: At this point, any Buffers created by GS's and resources (e.g. VertexStreams) have had their platform
		// objects created (although their data has not been buffered), and new resources have been API-created
		m_bindlessResourceManager.Update(currentFrame);
	}


	void Context::Destroy(re::Context& context)
	{
		dx12::Context& dx12Context = dynamic_cast<dx12::Context&>(context);

		if (dx12Context.m_pixGPUCaptureModule != nullptr)
		{
			LOG("Destroying PIX GPU programmatic capture module");
			FreeLibrary(dx12Context.m_pixGPUCaptureModule);
		}
		if (dx12Context.m_pixCPUCaptureModule != nullptr)
		{
			LOG("Destroying PIX CPU programmatic capture module");
			FreeLibrary(dx12Context.m_pixCPUCaptureModule);
		}

		// Make sure our command queues have finished all commands before closing.
		dx12Context.m_commandQueues[dx12::CommandListType::Copy].Flush();
		dx12Context.m_commandQueues[dx12::CommandListType::Copy].Destroy();
		
		dx12Context.m_commandQueues[dx12::CommandListType::Direct].Flush();
		dx12Context.m_commandQueues[dx12::CommandListType::Direct].Destroy();

		// NOTE: We must destroy anything that holds a buffer before the BufferAllocator is destroyed, 
		// as buffers call the BufferAllocator in their destructor
		dx12Context.GetBufferAllocator()->Destroy();

		dx12Context.m_bindlessResourceManager.Destroy();

		// Clear the null descriptor libraries:
		{
			std::scoped_lock lock(
				dx12Context.m_nullSRVLibraryMutex, dx12Context.m_nullUAVLibraryMutex, dx12Context.m_nullCBVMutex);

			dx12Context.s_nullSRVLibrary.clear();
			dx12Context.s_nullUAVLibrary.clear();
			dx12Context.m_nullCBV.Free(0); // Release immediately
		}

		// DX12 buffers contain cpu descriptors, so we must destroy the cpu descriptor heap manager after the
		// buffer allocator
		dx12Context.m_cpuDescriptorHeapMgrs.clear();

		{
			std::lock_guard<std::mutex> psoLibraryLock(dx12Context.m_PSOLibraryMutex);
			dx12Context.m_PSOLibrary.clear();
		}

		{
			std::lock_guard<std::mutex> rootSigLibraryLock(dx12Context.m_rootSigLibraryMutex);
			dx12Context.m_rootSigLibrary.clear();
		}

		// The heap manager can only be destroyed after all GPUResources have been released
		dx12Context.m_heapManager.Destroy();

		dx12Context.m_device.Destroy();
	}


	void Context::Present()
	{
		SEBeginCPUEvent("Context::Present");

		// Create a command list to transition the backbuffer to the presentation state
		dx12::CommandQueue& directQueue = m_commandQueues[dx12::CommandListType::Direct];

		// Get our swapchain and associated target set:
		re::SwapChain& swapChain = GetSwapChain();

		dx12::SwapChain::PlatformParams* swapChainPlatParams =
			swapChain.GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		std::shared_ptr<re::TextureTargetSet> const& swapChainTargetSet = SwapChain::GetBackBufferTargetSet(swapChain);

		// Transition our current backbuffer target set resource to the present state:
		std::shared_ptr<dx12::CommandList> directCmdList = directQueue.GetCreateCommandList();

#if defined(DEBUG_CMD_LIST_LOG_STAGE_NAMES)
		directCmdList->RecordStageName("<Present>");
#endif

		SEBeginGPUEvent(directCmdList->GetD3DCommandList().Get(), perfmarkers::Type::GraphicsCommandList, "Swapchain transitions");

		directCmdList->TransitionResource(
			swapChainTargetSet->GetColorTarget(0).GetTexture(),
			D3D12_RESOURCE_STATE_PRESENT,
			swapChainTargetSet->GetColorTarget(0).GetTargetParams().m_textureView);

		SEEndGPUEvent(directCmdList->GetD3DCommandList().Get());

		directQueue.Execute(1, &directCmdList);

		// Present the backbuffer:
		const bool vsyncEnabled = swapChainPlatParams->m_vsyncEnabled;
		const uint32_t syncInterval = vsyncEnabled ? 1 : 0;
		const uint32_t presentFlags = 
			(swapChainPlatParams->m_tearingSupported && !vsyncEnabled) ? DXGI_PRESENT_ALLOW_TEARING : 0;

		const HRESULT hr = swapChainPlatParams->m_swapChain->Present(syncInterval, presentFlags);
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
		const uint8_t currentFrameBackbufferIdx = dx12::SwapChain::GetCurrentBackBufferIdx(swapChain);
		m_frameFenceValues[currentFrameBackbufferIdx] = directQueue.GPUSignal();

		const uint8_t nextFrameBackbufferIdx = dx12::SwapChain::IncrementBackBufferIdx(swapChain);
		
		// Block the CPU on the fence for our new backbuffer, to ensure all of its work is done
		SEBeginCPUEvent("Context::Present: Frame fence CPU wait");
		directQueue.CPUWait(m_frameFenceValues[nextFrameBackbufferIdx]);
		SEEndCPUEvent();

		// Free the descriptors used on the next backbuffer now that we know the fence has been reached:
		for (size_t i = 0; i < CPUDescriptorHeapManager::HeapType_Count; i++)
		{
			m_cpuDescriptorHeapMgrs[static_cast<CPUDescriptorHeapManager::HeapType>(i)].ReleaseFreedAllocations(
				m_frameFenceValues[nextFrameBackbufferIdx]);
		}

		SEEndCPUEvent();
	}


	dx12::PipelineState const* Context::CreateAddPipelineState(
		re::Shader const& shader,
		re::TextureTargetSet const* targetSet)
	{
		std::shared_ptr<dx12::PipelineState> pso = nullptr;

		const uint64_t psoKey = ComputePSOKey(shader, targetSet);

		{
			std::lock_guard<std::mutex> lock(m_PSOLibraryMutex);

			if (m_PSOLibrary.contains(psoKey))
			{
				pso = m_PSOLibrary.at(psoKey);
			}
		}

		if (pso == nullptr)
		{
			pso = std::make_shared<dx12::PipelineState>();
			pso->Create(shader, targetSet);

			{
				std::lock_guard<std::mutex> lock(m_PSOLibraryMutex);

				m_PSOLibrary.emplace(psoKey, pso);
			}
		}
		return pso.get();
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


	dx12::PipelineState const* Context::GetPipelineStateObject(
		re::Shader const& shader, re::TextureTargetSet const* targetSet)
	{
		const uint64_t psoKey = ComputePSOKey(shader, targetSet);
		
		{
			std::lock_guard<std::mutex> lock(m_PSOLibraryMutex);

			if (m_PSOLibrary.contains(psoKey))
			{
				return m_PSOLibrary[psoKey].get();
			}
		}
		
		LOG_WARNING("Creating DX12 PSO for Shader \"%s\", TextureTargetSet \"%s\"",
			shader.GetName().c_str(),
			targetSet ? targetSet->GetName().c_str() : "<null TextureTargetSet>");

		return CreateAddPipelineState(shader, targetSet);
	}


	bool Context::HasRootSignature(uint64_t rootSigDescHash)
	{
		{
			std::lock_guard<std::mutex> lock(m_rootSigLibraryMutex);
			return m_rootSigLibrary.contains(rootSigDescHash);
		}
	}


	Microsoft::WRL::ComPtr<ID3D12RootSignature> Context::GetRootSignature(uint64_t rootSigDescHash)
	{
		SEAssert(HasRootSignature(rootSigDescHash), "Root signature has not been added");

		{
			std::lock_guard<std::mutex> lock(m_rootSigLibraryMutex);
			return m_rootSigLibrary[rootSigDescHash];
		}
	}


	void Context::AddRootSignature(uint64_t rootSigDescHash, Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig)
	{
		{
			std::lock_guard<std::mutex> lock(m_rootSigLibraryMutex);
			auto result = m_rootSigLibrary.insert({ rootSigDescHash, rootSig });
			SEAssert(result.second, "Root signature has already been added");
		}
	}


	DescriptorAllocation const& Context::GetNullSRVDescriptor(D3D12_SRV_DIMENSION dimension, DXGI_FORMAT format)
	{
		std::unique_lock<std::mutex> lock(m_nullSRVLibraryMutex);

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
			case D3D12_SRV_DIMENSION_BUFFER:
			{
				srvDesc.Buffer = D3D12_BUFFER_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURE1D:
			{
				srvDesc.Texture1D = D3D12_TEX1D_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
			{
				srvDesc.Texture1DArray = D3D12_TEX1D_ARRAY_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURE2D:
			{
				srvDesc.Texture2D = D3D12_TEX2D_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
			{
				srvDesc.Texture2DArray = D3D12_TEX2D_ARRAY_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMS:
			{
				srvDesc.Texture2DMS = D3D12_TEX2DMS_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
			{
				srvDesc.Texture2DMSArray = D3D12_TEX2DMS_ARRAY_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURE3D:
			{
				srvDesc.Texture3D = D3D12_TEX3D_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURECUBE:
			{
				srvDesc.TextureCube = D3D12_TEXCUBE_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
			{
				srvDesc.TextureCubeArray = D3D12_TEXCUBE_ARRAY_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
			{
				srvDesc.RaytracingAccelerationStructure = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_SRV{ 0 };
			}
			break;
			case D3D12_SRV_DIMENSION_UNKNOWN:
			default:
				SEAssertF("Invalid dimension");
			}


			DescriptorAllocation descriptor =
				std::move(m_cpuDescriptorHeapMgrs[CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV].Allocate(1));

			Microsoft::WRL::ComPtr<ID3D12Device> device = m_device.GetD3DDevice();

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
		std::unique_lock<std::mutex> lock(m_nullUAVLibraryMutex);

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
			case D3D12_UAV_DIMENSION_BUFFER:
			{
				uavDesc.Buffer = D3D12_BUFFER_UAV{ 0 };
			}
			break;
			case D3D12_UAV_DIMENSION_TEXTURE1D:
			{
				uavDesc.Texture1D = D3D12_TEX1D_UAV{ 0 };
			}
			break;
			case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
			{
				uavDesc.Texture1DArray = D3D12_TEX1D_ARRAY_UAV{ 0 };
			}
			break;
			case D3D12_UAV_DIMENSION_TEXTURE2D:
			{
				uavDesc.Texture2D = D3D12_TEX2D_UAV{ 0 };
			}
			break;
			case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
			{
				uavDesc.Texture2DArray = D3D12_TEX2D_ARRAY_UAV{ 0 };
			}
			break;
			case D3D12_UAV_DIMENSION_TEXTURE2DMS:
			{
				uavDesc.Texture2DMS = D3D12_TEX2DMS_UAV{ 0 };
			}
			break;
			case D3D12_UAV_DIMENSION_TEXTURE2DMSARRAY:
			{
				uavDesc.Texture2DMSArray = D3D12_TEX2DMS_ARRAY_UAV{ 0 };
			}
			break;
			case D3D12_UAV_DIMENSION_TEXTURE3D:
			{
				uavDesc.Texture3D = D3D12_TEX3D_UAV{ 0 };
			}
			break;
			case D3D12_UAV_DIMENSION_UNKNOWN:
			default:
				SEAssertF("Invalid dimension");
			}


			DescriptorAllocation descriptor =
				std::move(m_cpuDescriptorHeapMgrs[CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV].Allocate(1));

			Microsoft::WRL::ComPtr<ID3D12Device> device = m_device.GetD3DDevice();

			device->CreateUnorderedAccessView(
				nullptr,
				nullptr,
				&uavDesc,
				descriptor.GetBaseDescriptor());

			formatResult = dimensionResult->second.emplace(format, std::move(descriptor)).first;
		}

		return formatResult->second;
	}


	DescriptorAllocation const& Context::GetNullCBVDescriptor()
	{
		if (!m_nullCBV.IsValid())
		{
			std::unique_lock<std::mutex> lock(m_nullCBVMutex);

			if (!m_nullCBV.IsValid())
			{
				const D3D12_CONSTANT_BUFFER_VIEW_DESC nullCBVDesc
				{
					.BufferLocation = 0, // Null
					.SizeInBytes = 0,
				};

				m_nullCBV = 
					std::move(m_cpuDescriptorHeapMgrs[CPUDescriptorHeapManager::HeapType::CBV_SRV_UAV].Allocate(1));

				Microsoft::WRL::ComPtr<ID3D12Device> device = m_device.GetD3DDevice();

				device->CreateConstantBufferView(&nullCBVDesc, m_nullCBV.GetBaseDescriptor());
			}
		}
		return m_nullCBV;
	}
}