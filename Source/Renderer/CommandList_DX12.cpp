// © 2022 Adam Badke. All rights reserved.
#include "AccelerationStructure_DX12.h"
#include "Batch.h"
#include "BindlessResourceManager_DX12.h"
#include "Buffer.h"
#include "Buffer_DX12.h"
#include "CommandList_DX12.h"
#include "Debug_DX12.h"
#include "MeshPrimitive.h"
#include "PipelineState_DX12.h"
#include "RenderManager.h"
#include "RenderManager_DX12.h"
#include "RootConstants.h"
#include "RootSignature_DX12.h"
#include "ShaderBindingTable_DX12.h"
#include "SysInfo_DX12.h"
#include "Texture.h"
#include "Texture_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_DX12.h"
#include "TextureView.h"

#include "Core/Config.h"

#include "Core/Util/CastUtils.h"

#include <d3dx12_core.h>
#include <d3dx12_resource_helpers.h>

using Microsoft::WRL::ComPtr;


namespace
{
#if defined(DEBUG_CMD_LIST_RESOURCE_TRANSITIONS)
	void DebugResourceTransitions(
		dx12::CommandList const& cmdList,
		char const* resourceName,
		D3D12_RESOURCE_STATES fromState,
		D3D12_RESOURCE_STATES toState,
		uint32_t subresourceIdx,
		bool isPending = false)
	{
		char const* fromStateCStr = isPending ? "PENDING" : dx12::GetResourceStateAsCStr(fromState);
		const bool isSkipping = !isPending && (fromState == toState);
	
		// Cut down on log spam by filtering output containing keyword substrings
		if (ShouldSkipDebugOutput(resourceName))
		{
			return;
		}

		std::string const& debugStr = std::format("{}: Texture \"{}\", mip {}\n{}{} -> {}",
			dx12::GetDebugName(cmdList.GetD3DCommandList().Get()).c_str(),
			resourceName,
			subresourceIdx,
			(isSkipping ? "\t\tSkip: " : "\t"),
			(isPending ? "PENDING" : dx12::GetResourceStateAsCStr(fromState)),
			dx12::GetResourceStateAsCStr(toState)
		);

		LOG_WARNING(debugStr.c_str());
	}

	void DebugResourceTransitions(
		dx12::CommandList const& cmdList,
		char const* resourceName,
		D3D12_RESOURCE_STATES toState,
		uint32_t subresourceIdx)
	{
		DebugResourceTransitions(cmdList, resourceName, toState, toState, subresourceIdx, true);
	}
#endif


	ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(
		Microsoft::WRL::ComPtr<ID3D12Device> const& device, D3D12_COMMAND_LIST_TYPE type, std::wstring const& name)
	{
		SEAssert(device, "Device cannot be null");

		ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;

		HRESULT hr = device->CreateCommandAllocator(
			type, // Copy, compute, direct draw, etc
			IID_PPV_ARGS(&commandAllocator)); // IID_PPV_ARGS: RIID & interface pointer
		dx12::CheckHResult(hr, "Failed to create command allocator");

		commandAllocator->SetName(name.c_str());

		hr = commandAllocator->Reset();
		dx12::CheckHResult(hr, "Failed to reset command allocator");

		return commandAllocator;
	}


	constexpr D3D_PRIMITIVE_TOPOLOGY TranslateToD3DPrimitiveTopology(gr::MeshPrimitive::PrimitiveTopology topologyMode)
	{
		switch (topologyMode)
		{
		case gr::MeshPrimitive::PrimitiveTopology::PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		case gr::MeshPrimitive::PrimitiveTopology::LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case gr::MeshPrimitive::PrimitiveTopology::LineStrip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case gr::MeshPrimitive::PrimitiveTopology::TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case gr::MeshPrimitive::PrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case gr::MeshPrimitive::PrimitiveTopology::LineListAdjacency: return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
		case gr::MeshPrimitive::PrimitiveTopology::LineStripAdjacency: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
		case gr::MeshPrimitive::PrimitiveTopology::TriangleListAdjacency: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
		case gr::MeshPrimitive::PrimitiveTopology::TriangleStripAdjacency: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
		default:
			SEAssertF("Invalid topology mode");
			return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}


	inline uint32_t GetNumSubresources(ID3D12Resource* resource, ID3D12Device* device)
	{
		D3D12_RESOURCE_DESC const& desc = resource->GetDesc();
		const uint32_t planeCount = D3D12GetFormatPlaneCount(device, desc.Format);

		return planeCount * desc.DepthOrArraySize * desc.MipLevels;
	}
}


namespace dx12
{
	size_t CommandList::s_commandListNumber = 0;


	constexpr CommandListType CommandList::TranslateToSECommandListType(D3D12_COMMAND_LIST_TYPE type)
	{
		switch (type)
		{
		case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT: return CommandListType::Direct;
		case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_BUNDLE: return CommandListType::Bundle;
		case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE: return CommandListType::Compute;
		case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY: return CommandListType::Copy;
		default:
			SEAssertF("Invalid command list type");
		}
		return CommandListType::CommandListType_Invalid;
	}


	CommandList::CommandList(Microsoft::WRL::ComPtr<ID3D12Device> const& device, CommandListType type)
		: m_commandList(nullptr)
		, m_commandAllocator(nullptr)
		, m_commandAllocatorReuseFenceValue(0)
		, m_device(device.Get())
		, k_commandListNumber(s_commandListNumber++)
		, m_d3dType(TranslateToD3DCommandListType(type))
		, m_type(type)
		, m_gpuCbvSrvUavDescriptorHeap(nullptr)
		, m_currentRootSignature(nullptr)
		, m_currentPSO(nullptr)
	{
		SEAssert(m_device, "Device cannot be null");

		// Name the command list with a monotonically-increasing index to make it easier to identify
		const std::wstring commandListname = std::wstring(
			GetCommandListTypeWName(type)) +
			L"_CommandList_#" + std::to_wstring(k_commandListNumber);

		m_commandAllocator = CreateCommandAllocator(device, m_d3dType, commandListname + L"_CommandAllocator");

		// Create the command list:
		HRESULT hr = device->CreateCommandList(
			dx12::SysInfo::GetDeviceNodeMask(),
			m_d3dType,						// Direct draw/compute/copy/etc
			m_commandAllocator.Get(),		// The command allocator the command lists will be created on
			nullptr,						// Optional: Command list initial pipeline state
			IID_PPV_ARGS(&m_commandList));	// IID_PPV_ARGS: RIID & destination for the populated command list
		CheckHResult(hr, "Failed to create command list");

		m_commandList->SetName(commandListname.c_str());

		// Set the descriptor heaps (unless we're a copy command list):
		if (m_d3dType != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY)
		{
			// Create our GPU-visible descriptor heaps:
			m_gpuCbvSrvUavDescriptorHeap = std::make_unique<GPUDescriptorHeap>(
				k_gpuDescriptorHeapSize,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
				commandListname + L"_GPUDescriptorHeap");
		}

		// Note: Command lists are created in the recording state by default. The render loop resets the command 
		// list, which requires the command list to be closed. So, we pre-close new command lists so they're ready
		// to be reset before recording
		hr = m_commandList->Close();
		CheckHResult(hr, "Failed to close command list");
	}


	void CommandList::Destroy()
	{
		m_commandList = nullptr;
		m_type = CommandListType_Invalid;
		m_d3dType = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_NONE;
		m_commandAllocator = nullptr;
		m_commandAllocatorReuseFenceValue = 0;
		m_gpuCbvSrvUavDescriptorHeap = nullptr;
		m_currentRootSignature = nullptr;
		m_currentPSO = nullptr;
	}


	void CommandList::Reset()
	{
		m_currentRootSignature = nullptr;
		m_currentPSO = nullptr;

		// Reset the command allocator BEFORE we reset the command list (to avoid leaking memory)
		CheckHResult(
			m_commandAllocator->Reset(),
			"Failed to reset command allocator");

		m_resourceStates.Reset();

		// Note: pso is optional here; nullptr sets a dummy PSO
		CheckHResult(
			m_commandList->Reset(m_commandAllocator.Get(), nullptr),
			"Failed to reset command list");

		// Re-bind the descriptor heaps (unless we're a copy command list):
		if (m_d3dType != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY)
		{
			// Reset the GPU descriptor heap managers:
			m_gpuCbvSrvUavDescriptorHeap->Reset();

			ID3D12DescriptorHeap* descriptorHeap = m_gpuCbvSrvUavDescriptorHeap->GetD3DDescriptorHeap();
			m_commandList->SetDescriptorHeaps(1, &descriptorHeap);
		}

		m_commandAllocatorReuseFenceValue = 0;

		m_seenReadbackResources.clear();

#if defined(DEBUG_CMD_LIST_LOG_STAGE_NAMES)
		m_debugRecordedStages.clear();
#endif
	}


	void CommandList::SetPipelineState(dx12::PipelineState const& pso)
	{
		if (m_currentPSO == &pso)
		{
			return;
		}
		m_currentPSO = &pso;

		ID3D12PipelineState* pipelineState = pso.GetD3DPipelineState();
		SEAssert(pipelineState, "Pipeline state is null. This is unexpected");

		m_commandList->SetPipelineState(pipelineState);
	}


	void CommandList::SetGraphicsRootSignature(dx12::RootSignature const* rootSig)
	{
		SEAssert(m_d3dType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT,
			"Only graphics command lists can have a graphics/direct root signature");

		if (m_currentRootSignature == rootSig)
		{
			return;
		}
		m_currentRootSignature = rootSig;

		m_gpuCbvSrvUavDescriptorHeap->SetRootSignature(rootSig);

		ID3D12RootSignature* rootSignature = rootSig->GetD3DRootSignature();
		SEAssert(rootSignature, "Root signature is null. This is unexpected");

		m_commandList->SetGraphicsRootSignature(rootSignature);
	}


	void CommandList::SetComputeRootSignature(dx12::RootSignature const* rootSig)
	{
		SEAssert(m_d3dType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT || 
			m_d3dType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE,
			"Only graphics or compute command lists can have a compute root signature");

		if (m_currentRootSignature == rootSig)
		{
			return;
		}
		m_currentRootSignature = rootSig;

		m_gpuCbvSrvUavDescriptorHeap->SetRootSignature(rootSig);

		ID3D12RootSignature* rootSignature = rootSig->GetD3DRootSignature();
		SEAssert(rootSignature, "Root signature is null. This is unexpected");

		m_commandList->SetComputeRootSignature(rootSignature);
	}


	void CommandList::SetRootConstants(re::RootConstants const& rootConstants) const
	{
		SEAssert(m_d3dType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT ||
			m_d3dType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE,
			"Only graphics or compute command lists can set root constants");

		SEAssert(m_currentRootSignature, "Root signature has not been set");

		for (uint8_t i = 0; i < rootConstants.GetRootConstantCount(); ++i)
		{
			RootSignature::RootParameter const* rootParam =
				m_currentRootSignature->GetRootSignatureEntry(rootConstants.GetShaderName(i));
			SEAssert(rootParam ||
				core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				"Invalid root signature entry");

			if (rootParam)
			{
				const uint8_t rootIdx = rootParam->m_index;

				const uint32_t num32BitValues = DataTypeToNumComponents(rootConstants.GetDataType(i));
				
				SEAssert(num32BitValues > 0 && num32BitValues <= 4, "Invalid number of 32 bit values");
				
				switch (m_type)
				{
				case dx12::CommandListType::Direct:
				{
					m_commandList->SetGraphicsRoot32BitConstants(
						rootIdx,
						num32BitValues,
						rootConstants.GetValue(i),
						0);
				}
				break;
				case dx12::CommandListType::Compute:
				{
					m_commandList->SetComputeRoot32BitConstants(
						rootIdx,
						num32BitValues,
						rootConstants.GetValue(i),
						0);
				}
				break;
				default: SEAssertF("Invalid command list type");
				}
			}
		}
	}


	void CommandList::SetBuffers(std::vector<re::BufferInput> const& bufferInputs)
	{
		SEAssert(m_currentRootSignature, "Root signature has not been set");

		SEAssert(m_type == CommandListType::Direct || m_type == CommandListType::Compute,
			"Unexpected command list type for setting a buffer on");

		// Batch our resource transitions into a single call:
		std::vector<TransitionMetadata> resourceTransitions;
		resourceTransitions.reserve(bufferInputs.size());

		for (re::BufferInput const& bufferInput : bufferInputs)
		{
			re::Buffer const* buffer = bufferInput.GetBuffer();
			dx12::Buffer::PlatformParams* bufferPlatParams =
				buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

			RootSignature::RootParameter const* rootParam =
				m_currentRootSignature->GetRootSignatureEntry(bufferInput.GetShaderName());
			SEAssert(rootParam ||
				core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				"Invalid root signature entry");

			if (rootParam)
			{
				bool transitionResource = false;
				D3D12_RESOURCE_STATES toState = D3D12_RESOURCE_STATE_COMMON; // Updated below

				re::Buffer::BufferParams const& bufferParams = buffer->GetBufferParams();

				// Don't transition resources representing shared heaps
				const bool isInSharedHeap = bufferParams.m_lifetime == re::Lifetime::SingleFrame;

				switch (rootParam->m_type)
				{
				case RootSignature::RootParameter::Type::Constant:
				{
					SEAssertF("Unexpected root parameter type for a buffer");
				}
				break;
				case RootSignature::RootParameter::Type::CBV:
				{
					SEAssert(re::Buffer::HasUsageBit(re::Buffer::Constant, bufferParams),
						"Buffer is missing the Constant usage bit");
					SEAssert(rootParam->m_type == RootSignature::RootParameter::Type::CBV,
						"Unexpected root signature type");
					SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams) &&
						!re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams),
						"Invalid usage flags for a constant buffer");

					m_gpuCbvSrvUavDescriptorHeap->SetInlineCBV(
						rootParam->m_index,
						bufferPlatParams->m_resolvedGPUResource,
						bufferPlatParams->m_heapByteOffset);

					toState = (m_type == dx12::CommandListType::Compute ?
						D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

					transitionResource = !isInSharedHeap;
				}
				break;
				case RootSignature::RootParameter::Type::SRV:
				{
					SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, bufferParams),
						"Buffer is missing the Structured usage bit");
					SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams),
						"SRV buffers must have GPU reads enabled");

					m_gpuCbvSrvUavDescriptorHeap->SetInlineSRV(
						rootParam->m_index,
						bufferPlatParams->m_resolvedGPUResource,
						bufferPlatParams->m_heapByteOffset);

					toState = (m_type == dx12::CommandListType::Compute ?
						D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

					transitionResource = !isInSharedHeap;
				}
				break;
				case RootSignature::RootParameter::Type::UAV:
				{
					SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, bufferParams),
						"Buffer is missing the Structured usage bit");
					SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams),
						"UAV buffers must have GPU writes enabled");
					SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::Structured, bufferParams),
						"Buffer is missing the Structured usage bit");

					m_gpuCbvSrvUavDescriptorHeap->SetInlineUAV(
						rootParam->m_index,
						bufferPlatParams->m_resolvedGPUResource,
						bufferPlatParams->m_heapByteOffset);

					toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					transitionResource = true;
				}
				break;
				case RootSignature::RootParameter::Type::DescriptorTable:
				{
					re::BufferView const& bufView = bufferInput.GetView();

					D3D12_CPU_DESCRIPTOR_HANDLE tableDescriptor{};
					switch (rootParam->m_tableEntry.m_type)
					{
					case dx12::RootSignature::DescriptorType::CBV:
					{
						SEAssert(re::Buffer::HasUsageBit(re::Buffer::Constant, bufferParams),
							"Buffer is missing the Constant usage bit");
						SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams) &&
							!re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams),
							"Invalid usage flags for a constant buffer");

						tableDescriptor = dx12::Buffer::GetCBV(bufferInput.GetBuffer(), bufView);

						toState = (m_type == dx12::CommandListType::Compute ?
							D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

						transitionResource = !isInSharedHeap;
					}
					break;
					case dx12::RootSignature::DescriptorType::SRV:
					{
						SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::Structured, bufferParams),
							"Buffer is missing the Structured usage bit");
						SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams),
							"SRV buffers must have GPU reads enabled");
						SEAssert(bufferPlatParams->m_heapByteOffset == 0, "Unexpected heap byte offset");

						tableDescriptor = dx12::Buffer::GetSRV(bufferInput.GetBuffer(), bufView);

						toState = (m_type == dx12::CommandListType::Compute ?
							D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
						transitionResource = !isInSharedHeap;
					}
					break;
					case dx12::RootSignature::DescriptorType::UAV:
					{
						SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, bufferParams),
							"Buffer is missing the Structured usage bit");
						SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams),
							"UAV buffers must have GPU writes enabled");
						SEAssert(bufferPlatParams->m_heapByteOffset == 0, "Unexpected heap byte offset");

						tableDescriptor = dx12::Buffer::GetUAV(bufferInput.GetBuffer(), bufView);

						toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						transitionResource = true;
					}
					break;
					default: SEAssertF("Invalid type");
					}

					m_gpuCbvSrvUavDescriptorHeap->SetDescriptorTableEntry(
						rootParam->m_index,
						tableDescriptor,
						rootParam->m_tableEntry.m_offset + bufView.m_buffer.m_firstDestIdx,
						1);
				}
				break;
				default: SEAssertF("Invalid root parameter type");
				}

				if (transitionResource)
				{
					SEAssert(!isInSharedHeap, "Trying to transition a resource in a shared heap. This is unexpected");
					SEAssert(toState != D3D12_RESOURCE_STATE_COMMON, "Unexpected to state");

					resourceTransitions.emplace_back(TransitionMetadata{
						.m_resource = bufferPlatParams->m_resolvedGPUResource,
						.m_toState = toState,
						.m_subresourceIndexes = { D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES }
						});
				}

				// If our buffer has CPU readback enabled, add it to our tracking list so we can schedule a copy later on:
				if (re::Buffer::HasAccessBit(re::Buffer::CPURead, bufferParams))
				{
					const uint8_t readbackIdx = dx12::RenderManager::GetIntermediateResourceIdx();

					m_seenReadbackResources.emplace_back(ReadbackResourceMetadata{
						.m_srcResource = bufferPlatParams->m_resolvedGPUResource,
						.m_dstResource = bufferPlatParams->m_readbackResources[readbackIdx].m_readbackGPUResource->Get(),
						.m_dstModificationFence = &bufferPlatParams->m_readbackResources[readbackIdx].m_readbackFence,
						.m_dstModificationFenceMutex = &bufferPlatParams->m_readbackResources[readbackIdx].m_readbackFenceMutex });
				}
			}
		}

		// Finally, submit all of our resource transitions in a single batch
		TransitionResourcesInternal(std::move(resourceTransitions));
	}


	void CommandList::SetBuffers(std::vector<re::BufferInput> const& bufferInputs, re::ShaderBindingTable const& sbt)
	{
		dx12::ShaderBindingTable::SetBuffersOnLocalRoots(
			sbt,
			bufferInputs,
			this,
			m_gpuCbvSrvUavDescriptorHeap.get(),
			re::RenderManager::Get()->GetCurrentRenderFrameNum());
	}
	

	void CommandList::Dispatch(glm::uvec3 const& threadDimensions)
	{
		SEAssert(threadDimensions.x < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION &&
			threadDimensions.y < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION &&
			threadDimensions.z < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION,
			"Invalid dispatch dimensions");

		CommitGPUDescriptors();

		m_commandList->Dispatch(threadDimensions.x, threadDimensions.y, threadDimensions.z);
	}


	void CommandList::DispatchRays(
		re::ShaderBindingTable const& sbt, glm::uvec3 const& threadDimensions, uint32_t rayGenShaderIdx)
	{
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList4;
		CheckHResult(m_commandList.As(&commandList4), "Failed to get a ID3D12GraphicsCommandList4");

		dx12::ShaderBindingTable::PlatformParams const* sbtPlatParams = 
			sbt.GetPlatformParams()->As<dx12::ShaderBindingTable::PlatformParams const*>();
		
		commandList4->SetPipelineState1(sbtPlatParams->m_rayTracingStateObject.Get());

		// Note: All descriptors are (currently) set via local root signatures in the shader binding table,
		// so no need to commit the GPU descriptor heap here

		D3D12_DISPATCH_RAYS_DESC const& dispatchRaysDesc = dx12::ShaderBindingTable::BuildDispatchRaysDesc(
			sbt, threadDimensions, re::RenderManager::Get()->GetCurrentRenderFrameNum(), rayGenShaderIdx);

		commandList4->DispatchRays(&dispatchRaysDesc);
	}


	void CommandList::DrawBatchGeometry(re::Batch const& batch)
	{
		// Set the geometry for the draw:		
		re::Batch::GraphicsParams const& batchGraphicsParams = batch.GetGraphicsParams();

		SetPrimitiveType(TranslateToD3DPrimitiveTopology(batchGraphicsParams.m_primitiveTopology));

		SetVertexBuffers(batchGraphicsParams.m_vertexBuffers);

		// Record the draw:
		switch (batchGraphicsParams.m_batchGeometryMode)
		{
		case re::Batch::GeometryMode::IndexedInstanced:
		{
			SEAssert(batchGraphicsParams.m_indexBuffer.GetBuffer(), "Index stream cannot be null for indexed draws");

			SetIndexBuffer(batchGraphicsParams.m_indexBuffer);

			CommitGPUDescriptors();
			
			m_commandList->DrawIndexedInstanced(
				batchGraphicsParams.m_indexBuffer.m_view.m_stream.m_numElements,	// Index count, per instance
				static_cast<uint32_t>(batch.GetInstanceCount()),		// Instance count
				0,														// Start index location
				0,														// Base vertex location
				0);														// Start instance location
		}
		break;
		case re::Batch::GeometryMode::ArrayInstanced:
		{		
			SEAssert(batchGraphicsParams.m_vertexBuffers[0].m_view.m_stream.m_type == 
				gr::VertexStream::Type::Position,
				"We're currently assuming the first stream contains the correct number of elements for the entire draw."
				" If you hit this, validate this logic and delete this assert");

			CommitGPUDescriptors();

			m_commandList->DrawInstanced(
				batchGraphicsParams.m_vertexBuffers[0].m_view.m_stream.m_numElements,	// VertexCountPerInstance
				batchGraphicsParams.m_numInstances,										// InstanceCount
				0,																		// StartVertexLocation
				0);																		// StartInstanceLocation
		}
		break;
		default: SEAssertF("Invalid batch geometry type");
		}
	}


	void CommandList::SetVertexBuffers(
		std::array<re::VertexBufferInput, gr::VertexStream::k_maxVertexStreams> const& vertexBuffers)
	{
		// Batch all of the resource transitions in advance:
		std::vector<TransitionMetadata> resourceTransitions;
		resourceTransitions.reserve(vertexBuffers.size());

		for (uint32_t streamIdx = 0; streamIdx < gr::VertexStream::k_maxVertexStreams; streamIdx++)
		{
			// We assume vertex streams will be tightly packed, with streams of the same type stored consecutively
			if (!vertexBuffers[streamIdx].GetStream())
			{
				SEAssert(streamIdx > 0, "Failed to find a valid vertex stream");
				break;
			}
			re::Buffer const* streamBuffer = vertexBuffers[streamIdx].GetBuffer();

			// Currently, single-frame buffers are held in a shared heap so we can't/don't need to transition them here
			if (streamBuffer->GetLifetime() != re::Lifetime::SingleFrame)
			{
				dx12::Buffer::PlatformParams* streamBufferPlatParams =
					streamBuffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

				resourceTransitions.emplace_back(TransitionMetadata{
					.m_resource = streamBufferPlatParams->m_resolvedGPUResource,
					.m_toState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
					.m_subresourceIndexes = { D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
					});
			}
		}
		TransitionResourcesInternal(std::move(resourceTransitions));


		std::vector<D3D12_VERTEX_BUFFER_VIEW> streamViews;
		streamViews.reserve(gr::VertexStream::k_maxVertexStreams);

		uint8_t startSlotIdx = vertexBuffers[0].m_bindSlot;
		uint8_t nextConsecutiveSlotIdx = startSlotIdx + 1;
		for (uint32_t streamIdx = 0; streamIdx < gr::VertexStream::k_maxVertexStreams; streamIdx++)
		{
			// We assume vertex streams will be tightly packed, with streams of the same type stored consecutively
			if (!vertexBuffers[streamIdx].GetStream())
			{
				SEAssert(streamIdx > 0, "Failed to find a valid vertex stream");
				break;
			}
			re::Buffer const* streamBuffer = vertexBuffers[streamIdx].GetBuffer();

			dx12::Buffer::PlatformParams* streamBufferPlatParams =
				streamBuffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();
			
			streamViews.emplace_back(
				*dx12::Buffer::GetOrCreateVertexBufferView(*streamBuffer, vertexBuffers[streamIdx].m_view));

			// Peek ahead: If there are no more contiguous slots, flush the stream views
			const uint32_t nextStreamIdx = streamIdx + 1;
			if (nextStreamIdx >= gr::VertexStream::k_maxVertexStreams ||
				vertexBuffers[nextStreamIdx].m_bindSlot != nextConsecutiveSlotIdx)
			{
				SEAssert(nextStreamIdx >= gr::VertexStream::k_maxVertexStreams ||
					vertexBuffers[nextStreamIdx].m_bindSlot > nextConsecutiveSlotIdx, 
					"Out of order vertex streams detected");

				// Flush the list we've built so far
				if (!streamViews.empty())
				{
					m_commandList->IASetVertexBuffers(
						startSlotIdx,
						static_cast<uint32_t>(streamViews.size()), 
						streamViews.data());

					streamViews.clear();
				}

				// Prepare for the next iteration:
				if (nextStreamIdx < gr::VertexStream::k_maxVertexStreams)
				{
					startSlotIdx = vertexBuffers[nextStreamIdx].m_bindSlot;
					uint8_t nextConsecutiveSlotIdx = startSlotIdx + 1;
				}
			}
			else
			{
				++nextConsecutiveSlotIdx;
			}
		}

		SEAssert(streamViews.empty(), "Unflushed vertex streams");
	}


	void CommandList::SetIndexBuffer(re::VertexBufferInput const& indexBuffer)
	{
		SEAssert(indexBuffer.GetStream(), "Index stream buffer is null");

		dx12::Buffer::PlatformParams* streamBufferPlatParams =
			indexBuffer.GetBuffer()->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		m_commandList->IASetIndexBuffer(
			dx12::Buffer::GetOrCreateIndexBufferView(*indexBuffer.GetBuffer(), indexBuffer.m_view));

		// Currently, single-frame buffers are held in a shared heap so we can't/don't need to transition them here
		if (indexBuffer.GetBuffer()->GetLifetime() != re::Lifetime::SingleFrame)
		{
			TransitionResourceInternal(
				streamBufferPlatParams->m_resolvedGPUResource,
				D3D12_RESOURCE_STATE_INDEX_BUFFER,
				{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES });
		}
	}


	void CommandList::ClearColorTargets(
		bool const* colorClearModes,
		glm::vec4 const* colorClearVals,
		uint8_t numColorClears,
		re::TextureTargetSet const& targetSet)
	{
		SEAssert(colorClearModes && colorClearVals && numColorClears > 0,
			"Invalid clear args");

		std::vector<re::TextureTarget> const& texTargets = targetSet.GetColorTargets();

		SEAssert(numColorClears == 1 || 
			(numColorClears > 0 && numColorClears == texTargets.size()),
			"Number of clear values doesn't match the number of texture targets");

		auto ClearColorTarget = [this](glm::vec4 const& clearVal, re::TextureTarget const* colorTarget)
			{
				SEAssert(colorTarget, "Target texture cannot be null");

				SEAssert((colorTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::ColorTarget) ||
					(colorTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::SwapchainColorProxy),
					"Target texture must be a color target");

				core::InvPtr<re::Texture> const& colorTargetTex = colorTarget->GetTexture();
				re::TextureTarget::TargetParams const& colorTargetParams = colorTarget->GetTargetParams();

				const D3D12_CPU_DESCRIPTOR_HANDLE targetDescriptor =
					dx12::Texture::GetRTV(colorTargetTex, colorTargetParams.m_textureView);

				m_commandList->ClearRenderTargetView(
					targetDescriptor,
					&clearVal.r,
					0,			// Number of rectangles in the proceeding D3D12_RECT ptr
					nullptr);	// Ptr to an array of rectangles to clear in the resource view. Clears entire view if null
			};

		// Batch resource transitions together in advance:
		std::vector<TransitionMetadata> resourceTransitions;
		resourceTransitions.reserve(texTargets.size());
		for (size_t i = 0; i < texTargets.size(); ++i)
		{
			if (!texTargets[i].HasTexture())
			{
				break; // Targets must be bound in monotonically-increasing order from slot 0
			}

			core::InvPtr<re::Texture> const& colorTargetTex = texTargets[i].GetTexture();
			re::TextureTarget::TargetParams const& colorTargetParams = texTargets[i].GetTargetParams();

			resourceTransitions.emplace_back(TransitionMetadata{
				.m_resource = colorTargetTex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>()->m_gpuResource->Get(),
				.m_toState = D3D12_RESOURCE_STATE_RENDER_TARGET,
				.m_subresourceIndexes = {re::TextureView::GetSubresourceIndex(colorTargetTex, colorTargetParams.m_textureView)},
				});
		}
		TransitionResourcesInternal(std::move(resourceTransitions));

		
		for (size_t i = 0; i < texTargets.size(); ++i)
		{
			if (!texTargets[i].HasTexture())
			{
				break; // Targets must be bound in monotonically-increasing order from slot 0
			}
			
			if (numColorClears == 1)
			{
				ClearColorTarget(colorClearVals[0], &texTargets[i]);
			}
			else if (colorClearModes[i])
			{
				ClearColorTarget(colorClearVals[i], &texTargets[i]);
			}
		}
	}


	void CommandList::ClearTargets(
		bool const* colorClearModes,
		glm::vec4 const* colorClearVals,
		uint8_t numColorClears,
		bool depthClearMode,
		float depthClearVal,
		bool stencilClearMode,
		uint8_t stencilClearVal,
		re::TextureTargetSet const& targetSet)
	{
		SEAssert((colorClearModes != nullptr) == (colorClearVals != nullptr) &&
			(colorClearModes != nullptr) == (numColorClears != 0),
			"Invalid color clear args");

		if (colorClearModes)
		{
			ClearColorTargets(colorClearModes, colorClearVals, numColorClears, targetSet);
		}

		if (targetSet.HasDepthTarget() && (depthClearMode || stencilClearMode))
		{
			ClearDepthStencilTarget(
				depthClearMode, depthClearVal, stencilClearMode, stencilClearVal, targetSet.GetDepthStencilTarget());
		}

		SEAssert(!stencilClearMode, "TODO: Support stencil clears");
	}


	void CommandList::ClearDepthStencilTarget(
		bool depthClearMode,
		float depthClearVal,
		bool stencilClearMode,
		uint8_t stencilClearVal,
		re::TextureTarget const& depthTarget)
	{
		SEAssert((depthClearMode || stencilClearMode) && depthTarget.HasTexture(), "Invalid depth/stencil clear params");

		core::InvPtr<re::Texture> const& depthTex = depthTarget.GetTexture();

		SEAssert((depthTex->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget) != 0 ||
			(depthTex->GetTextureParams().m_usage & re::Texture::Usage::StencilTarget) != 0 ||
			(depthTex->GetTextureParams().m_usage & re::Texture::Usage::DepthStencilTarget) != 0,
			"Target texture must be a depth or stencil target");

		re::TextureTarget::TargetParams const& depthTargetParams = depthTarget.GetTargetParams();

		SEAssert(depthTargetParams.m_textureView.DepthWritesEnabled(), "Texture view has depth writes disabled");

		// Ensure we're in a depth write state:
		TransitionResource(depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE, depthTargetParams.m_textureView);

		const D3D12_CPU_DESCRIPTOR_HANDLE targetDescriptor = 
			dx12::Texture::GetDSV(depthTex, depthTargetParams.m_textureView);

		D3D12_CLEAR_FLAGS clearFlags;
		if (depthClearMode && !stencilClearMode)
		{
			clearFlags = D3D12_CLEAR_FLAG_DEPTH;
		}
		else if (!depthClearMode && stencilClearMode)
		{
			clearFlags = D3D12_CLEAR_FLAG_STENCIL;
		}
		else
		{
			clearFlags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL;
		}

		m_commandList->ClearDepthStencilView(
			targetDescriptor,
			clearFlags,
			depthClearVal,
			stencilClearVal,
			0,
			nullptr);
	}


	void CommandList::ClearUAV(std::vector<re::RWTextureInput> const& rwTexInputs, glm::vec4 const& clearVal)
	{
		for (auto const& rwTexInput : rwTexInputs)
		{
			dx12::Texture::PlatformParams const* texPlatParams = 
				rwTexInput.m_texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

			D3D12_CPU_DESCRIPTOR_HANDLE const& texDescriptor = 
				dx12::Texture::GetUAV(rwTexInput.m_texture, rwTexInput.m_textureView);

			D3D12_GPU_DESCRIPTOR_HANDLE const& gpuVisibleTexDescriptor =
				m_gpuCbvSrvUavDescriptorHeap->CommitToGPUVisibleHeap({ texDescriptor });

			m_commandList->ClearUnorderedAccessViewFloat(
				gpuVisibleTexDescriptor, 
				texDescriptor,
				texPlatParams->m_gpuResource->Get(),
				&clearVal.x,
				0,			// NumRects: 0, as we currently just clear the whole resource
				nullptr);	// D3D12_RECT*
		}
	}


	void CommandList::ClearUAV(std::vector<re::RWTextureInput> const& rwTexInputs, glm::uvec4 const& clearVal)
	{
		for (auto const& rwTexInput : rwTexInputs)
		{
			dx12::Texture::PlatformParams const* texPlatParams =
				rwTexInput.m_texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

			D3D12_CPU_DESCRIPTOR_HANDLE const& texDescriptor =
				dx12::Texture::GetUAV(rwTexInput.m_texture, rwTexInput.m_textureView);

			D3D12_GPU_DESCRIPTOR_HANDLE const& gpuVisibleTexDescriptor =
				m_gpuCbvSrvUavDescriptorHeap->CommitToGPUVisibleHeap({ texDescriptor });

			m_commandList->ClearUnorderedAccessViewUint(
				gpuVisibleTexDescriptor,
				texDescriptor,
				texPlatParams->m_gpuResource->Get(),
				&clearVal.x,
				0,			// NumRects: 0, as we currently just clear the whole resource
				nullptr);	// D3D12_RECT*
		}
	}


	void CommandList::SetRenderTargets(re::TextureTargetSet const& targetSet)
	{
		SEAssert(m_type != CommandListType::Compute && m_type != CommandListType::Copy,
			"This method is not valid for compute or copy command lists");

		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> colorTargetDescriptors;
		colorTargetDescriptors.reserve(targetSet.GetColorTargets().size());

		for (uint8_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			re::TextureTarget const& target = targetSet.GetColorTarget(i);
			if (!target.HasTexture())
			{
				break; // Targets must be bound in monotonically-increasing order from slot 0
			}
			core::InvPtr<re::Texture> const& targetTexture = target.GetTexture();

			dx12::Texture::PlatformParams const* texPlatParams =
				targetTexture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

			re::TextureTarget::TargetParams const& targetParams = target.GetTargetParams();

			TransitionResource(targetTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, targetParams.m_textureView);

			// Attach the RTV for the target face:
			colorTargetDescriptors.emplace_back(dx12::Texture::GetRTV(targetTexture, targetParams.m_textureView));
		}


		D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptor{};

		re::TextureTarget const& depthStencilTarget = targetSet.GetDepthStencilTarget();
		const bool hasDepthTargetTex = depthStencilTarget.HasTexture();
		if (hasDepthTargetTex)
		{
			core::InvPtr<re::Texture> const& depthTex = depthStencilTarget.GetTexture();

			re::TextureTarget::TargetParams const& depthTargetParams = depthStencilTarget.GetTargetParams();

			const D3D12_RESOURCE_STATES depthState = depthTargetParams.m_textureView.DepthWritesEnabled() ?
				D3D12_RESOURCE_STATE_DEPTH_WRITE :
				(D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			dx12::Texture::PlatformParams const* texPlatParams =
				depthTex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

			TransitionResource(depthTex, depthState, depthTargetParams.m_textureView);

			dsvDescriptor = dx12::Texture::GetDSV(depthTex, depthTargetParams.m_textureView);
		}

		const uint32_t numColorTargets = targetSet.GetNumColorTargets();

		// NOTE: isSingleHandleToDescRange == true specifies that the rtvs are contiguous in memory, thus N rtv 
		// descriptors will be found by offsetting from rtvs[0]. Otherwise, it is assumed rtvs is an array of descriptor
		// pointers
		m_commandList->OMSetRenderTargets(
			numColorTargets,
			colorTargetDescriptors.data(),
			false,			// Our render target descriptors (currently) aren't guaranteed to be in a contiguous range
			hasDepthTargetTex ? &dsvDescriptor : nullptr);

		// Set the viewport and scissor rectangles:
		SetViewport(targetSet);
		SetScissorRect(targetSet);
	}


	void CommandList::SetRWTextures(std::vector<re::RWTextureInput> const& rwTexInputs)
	{
		SEAssert(m_type == CommandListType::Direct || m_type == CommandListType::Compute,
			"This function should only be called from direct or compute command lists");
		SEAssert(m_currentRootSignature, "Root signature is not currently set");

		// Batch our resource transitions together:
		std::vector<TransitionMetadata> resourceTransitions;
		resourceTransitions.reserve(rwTexInputs.size());

		for (size_t i = 0; i < rwTexInputs.size(); i++)
		{
			re::RWTextureInput const& rwTexInput = rwTexInputs[i];

			RootSignature::RootParameter const* rootParam =
				m_currentRootSignature->GetRootSignatureEntry(rwTexInput.m_shaderName);

			SEAssert(rootParam ||
				core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				"Invalid root signature entry");

			if (rootParam)
			{
				SEAssert(rootParam->m_type == RootSignature::RootParameter::Type::DescriptorTable,
					"We currently assume all textures belong to descriptor tables");

				SEAssert(rootParam->m_tableEntry.m_type == dx12::RootSignature::DescriptorType::UAV,
					"RW textures must be UAVs");

				core::InvPtr<re::Texture> const& rwTex = rwTexInput.m_texture;

				SEAssert(((rwTex->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget) == 0) &&
					((rwTex->GetTextureParams().m_usage & re::Texture::Usage::ColorTarget) != 0),
					"Unexpected texture usage for a RW texture");

				m_gpuCbvSrvUavDescriptorHeap->SetDescriptorTableEntry(
					rootParam->m_index,
					dx12::Texture::GetUAV(rwTex, rwTexInput.m_textureView),
					rootParam->m_tableEntry.m_offset,
					1);

				dx12::Texture::PlatformParams const* texPlatParams =
					rwTex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

				resourceTransitions.emplace_back(TransitionMetadata{
					.m_resource = texPlatParams->m_gpuResource->Get(),
					.m_toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					.m_subresourceIndexes = re::TextureView::GetSubresourceIndexes(
						rwTexInput.m_texture, rwTexInput.m_textureView),
					});
			}
		}

		// Finally, insert our batched resource transitions:
		TransitionResourcesInternal(std::move(resourceTransitions));
	}


	void CommandList::SetViewport(re::TextureTargetSet const& targetSet) const
	{
		SEAssert(m_type != CommandListType::Compute && m_type != CommandListType::Copy,
			"This method is not valid for compute or copy command lists");

		dx12::TextureTargetSet::PlatformParams* targetSetParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		const uint32_t numViewports = 1;
		m_commandList->RSSetViewports(numViewports, &targetSetParams->m_viewport);

		// TODO: It is possible to have more than 1 viewport (eg. Geometry shaders), we should handle this (i.e. a 
		// viewport per target?)
	}


	void CommandList::SetScissorRect(re::TextureTargetSet const& targetSet) const
	{
		dx12::TextureTargetSet::PlatformParams* targetSetParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		const uint32_t numScissorRects = 1; // 1 per viewport, in an array of viewports
		m_commandList->RSSetScissorRects(numScissorRects, &targetSetParams->m_scissorRect);
	}


	void CommandList::BuildRaytracingAccelerationStructure(re::AccelerationStructure& as, bool doUpdate)
	{
		switch (as.GetType())
		{
		case re::AccelerationStructure::Type::TLAS:
		{
			//
		}
		break;
		case re::AccelerationStructure::Type::BLAS:
		{
			re::AccelerationStructure::BLASParams const* createParams =
				dynamic_cast<re::AccelerationStructure::BLASParams const*>(as.GetASParams());
			SEAssert(createParams, "Failed to get AS create params");

			// Batch resource transitions together in advance:
			std::vector<TransitionMetadata> resourceTransitions;
			resourceTransitions.reserve(createParams->m_geometry.size());

			// Transition the inputs:
			for (auto const& instance : createParams->m_geometry)
			{
				SEAssert(instance.GetVertexPositions().GetBuffer()->GetLifetime() != re::Lifetime::SingleFrame,
					"Single frame buffers are held in a shared heap, we can't transition them. DXR requires vertex"
					"buffers to be in the D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE state");

				dx12::Buffer::PlatformParams* positionBufferPlatParams =
					instance.GetVertexPositions().GetBuffer()->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

				resourceTransitions.emplace_back(TransitionMetadata{
					.m_resource = positionBufferPlatParams->m_resolvedGPUResource,
					.m_toState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
					.m_subresourceIndexes = { D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
					});
				
				if (instance.GetVertexIndices())
				{
					SEAssert(instance.GetVertexIndices()->GetBuffer()->GetLifetime() != re::Lifetime::SingleFrame,
						"Single frame buffers are held in a shared heap, we can't transition them. DXR requires index"
						"buffers to be in the D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE state");

					dx12::Buffer::PlatformParams* indexBufferPlatParams =
						instance.GetVertexIndices()->GetBuffer()->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

					resourceTransitions.emplace_back(TransitionMetadata{
						.m_resource = indexBufferPlatParams->m_resolvedGPUResource,
						.m_toState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
						.m_subresourceIndexes = { D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						});
				}

				if (createParams->m_transform)
				{
					SEAssert(createParams->m_transform->GetLifetime() != re::Lifetime::SingleFrame,
						"Single frame buffers are held in a shared heap, we can't transition them. DXR requires "
						"buffers to be in the D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE state");

					dx12::Buffer::PlatformParams* bufferPlatParams =
						createParams->m_transform->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

					resourceTransitions.emplace_back(TransitionMetadata{
						.m_resource = bufferPlatParams->m_resolvedGPUResource,
						.m_toState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
						.m_subresourceIndexes = { D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES },
						});
				}
			}

			TransitionResourcesInternal(std::move(resourceTransitions));
		}
		break;
		default: SEAssertF("Invalid AS type");
		}

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> cmdList4;
		const HRESULT hr = m_commandList.As(&cmdList4);
		SEAssert(SUCCEEDED(hr), "Failed to get command list as ID3D12GraphicsCommandList4");

		dx12::AccelerationStructure::BuildAccelerationStructure(as, doUpdate, cmdList4.Get());

		// Add a barrier to prevent the AS from being accessed before the build is complete (E.g. if building a BLAS and
		// TLAS on the same command list)
		dx12::AccelerationStructure::PlatformParams* platParams =
			as.GetPlatformParams()->As<dx12::AccelerationStructure::PlatformParams*>();

		InsertUAVBarrier(platParams->m_ASBuffer->Get());
	}


	void CommandList::AttachBindlessResources(re::ShaderBindingTable const& sbt, re::BindlessResourceManager const& brm)
	{
		dx12::ShaderBindingTable::SetBindlessResourcesOnLocalRoots(
			sbt, 
			brm, 
			m_gpuCbvSrvUavDescriptorHeap.get(), 
			re::RenderManager::Get()->GetCurrentRenderFrameNum());

		// Transition resources:
		TransitionResources(dx12::BindlessResourceManager::BuildResourceTransitions(brm));
	}


	void CommandList::SetTLAS(re::ASInput const& tlas, re::ShaderBindingTable const& sbt)
	{
		dx12::ShaderBindingTable::SetTLASOnLocalRoots(
			sbt,
			tlas,
			m_gpuCbvSrvUavDescriptorHeap.get(),
			re::RenderManager::Get()->GetCurrentRenderFrameNum());

		// Set the bindless LUT on the global root sig:
		re::AccelerationStructure::TLASParams const* tlasParams =
			dynamic_cast<re::AccelerationStructure::TLASParams const*>(tlas.m_accelerationStructure->GetASParams());
		SEAssert(tlasParams, "Failed to get TLASParams");

		// Set the bindless LUT buffer:
		SetBuffers({ tlasParams->GetBindlessResourceLUT() }, sbt);
	}


	void CommandList::SetRWTextures(
		std::vector<re::RWTextureInput> const& rwTexInputs, re::ShaderBindingTable const& sbt)
	{
		// Batch our resource transitions together:
		std::vector<TransitionMetadata> resourceTransitions;
		resourceTransitions.reserve(rwTexInputs.size());

		for (auto const& rwTexInput : rwTexInputs)
		{
			core::InvPtr<re::Texture> const& rwTex = rwTexInput.m_texture;

			dx12::Texture::PlatformParams const* texPlatParams =
				rwTex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

			resourceTransitions.emplace_back(TransitionMetadata{
				.m_resource = texPlatParams->m_gpuResource->Get(),
				.m_toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				.m_subresourceIndexes = re::TextureView::GetSubresourceIndexes(
					rwTexInput.m_texture, rwTexInput.m_textureView),
				});

			// DXR: We set our descriptors via the shader binding table (which internally stages them in our GPU-visible
			// descriptor heap)
			dx12::ShaderBindingTable::SetRWTextureOnLocalRoots(
				sbt,
				rwTexInput,
				m_gpuCbvSrvUavDescriptorHeap.get(),
				re::RenderManager::Get()->GetCurrentRenderFrameNum());
		}

		// Finally, insert our batched resource transitions:
		TransitionResourcesInternal(std::move(resourceTransitions));
	}


	void CommandList::UpdateSubresource(
		core::InvPtr<re::Texture> const& texture,
		uint32_t arrayIdx,
		uint32_t faceIdx,
		uint32_t mipLevel,
		ID3D12Resource* intermediate,
		size_t intermediateOffset) // Byte offset to start storing intermediate data at
	{
		SEAssert(m_type == dx12::CommandListType::Copy, "Expected a copy command list");

		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		dx12::Texture::PlatformParams const* texPlatParams =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();
	
		glm::vec4 const& mipDimensions = texture->GetMipLevelDimensions(mipLevel);
		const uint32_t texWidth = static_cast<uint32_t>(mipDimensions.x);

		const uint8_t bytesPerTexel = re::Texture::GetNumBytesPerTexel(texParams.m_format);
		const uint32_t bytesPerRow = bytesPerTexel * texWidth;
		const uint32_t numBytesPerFace = texture->GetTotalBytesPerFace(mipLevel);

		void const* initialData = texture->GetTexelData(arrayIdx, faceIdx);
		SEAssert(initialData, "Initial data cannot be null");

		const D3D12_SUBRESOURCE_DATA subresourceData = D3D12_SUBRESOURCE_DATA
		{
			.pData = initialData,

			// https://github.com/microsoft/DirectXTex/wiki/ComputePitch
			// Row pitch: The number of bytes in a scanline of pixels: bytes-per-pixel * width-of-image
			// - Can be larger than the number of valid pixels due to alignment padding
			.RowPitch = bytesPerRow,

			// Slice pitch: The number of bytes in each depth slice: No. bytes per pixel * width * height
			// - 1D/2D images: The total size of the image, including alignment padding
			// - 3D images: The size of 1 slice. NOTE: All slices for the subresource WILL be updated from the
			//		intermediate resource
			.SlicePitch = numBytesPerFace
		};

		// Transition to the copy destination state:
		const uint32_t subresourceIdx = texture->GetSubresourceIndex(arrayIdx, faceIdx, mipLevel);
		
		TransitionResourceInternal(
			texPlatParams->m_gpuResource->Get(), D3D12_RESOURCE_STATE_COPY_DEST, {subresourceIdx});

		// Record the update:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/updatesubresources2
		const uint64_t bufferSizeResult = ::UpdateSubresources(
			m_commandList.Get(),					// Command list
			texPlatParams->m_gpuResource->Get(),	// Destination resource
			intermediate,							// Intermediate resource
			0,										// Byte offset to the intermediate resource
			subresourceIdx,							// Index of 1st subresource in the resource
			1,										// Number of subresources in the subresources array
			&subresourceData);						// Array of subresource data structs
		SEAssert(bufferSizeResult > 0, "UpdateSubresources returned 0 bytes. This is unexpected");
	}


	void CommandList::UpdateSubresources(
		re::Buffer const* buffer, uint32_t dstOffset, ID3D12Resource* srcResource, uint32_t srcOffset, uint32_t numBytes)
	{
		SEAssert(m_type == dx12::CommandListType::Copy, "Expected a copy command list");
		SEAssert((buffer->GetBufferParams().m_memPoolPreference == re::Buffer::DefaultHeap),
			"Only expecting resources on the default heap to be updated via a copy queue");

		dx12::Buffer::PlatformParams const* bufferPlatformParams =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>();
		
		SEAssert(bufferPlatformParams->GPUResourceIsValid(),
			"GPUResource is not valid. Buffers using a shared resource cannot be used here");

		TransitionResourceInternal(
			bufferPlatformParams->m_resolvedGPUResource,
			D3D12_RESOURCE_STATE_COPY_DEST,
			{ D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES });

		m_commandList->CopyBufferRegion(
			bufferPlatformParams->m_resolvedGPUResource,	// pDstBuffer
			dstOffset,									// DstOffset
			srcResource,								// pSrcBuffer
			srcOffset,									// SrcOffset
			numBytes);									// NumBytes
	}


	void CommandList::CopyResource(ID3D12Resource* srcResource, ID3D12Resource* dstResource)
	{
		TransitionResourceInternal(srcResource, D3D12_RESOURCE_STATE_COPY_SOURCE, {D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES});
		TransitionResourceInternal(dstResource, D3D12_RESOURCE_STATE_COPY_DEST, {D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES});

		m_commandList->CopyResource(dstResource, srcResource);
	}


	void CommandList::CopyTexture(core::InvPtr<re::Texture> const& src, core::InvPtr<re::Texture> const& dst)
	{
		CopyResource(
			src->GetPlatformParams()->As<dx12::Texture::PlatformParams*>()->m_gpuResource->Get(),
			dst->GetPlatformParams()->As<dx12::Texture::PlatformParams*>()->m_gpuResource->Get());
	}


	void CommandList::SetTextures(
		std::vector<re::TextureAndSamplerInput> const& texInputs, int depthTargetTexInputIdx /*= -1*/)
	{
		SEAssert(m_currentPSO, "Pipeline is not currently set");

		SEAssert(m_d3dType == D3D12_COMMAND_LIST_TYPE_COMPUTE ||
			m_d3dType == D3D12_COMMAND_LIST_TYPE_DIRECT,
			"Unexpected command list type");

		// Batch our resource transitions into a single call:
		std::vector<TransitionMetadata> resourceTransitions;
		resourceTransitions.reserve(texInputs.size());

		for (size_t texIdx = 0; texIdx < texInputs.size(); texIdx++)
		{
			re::TextureAndSamplerInput const& texSamplerInput = texInputs[texIdx];

			RootSignature::RootParameter const* rootParam =
				m_currentRootSignature->GetRootSignatureEntry(texSamplerInput.m_shaderName);
			SEAssert(rootParam ||
				core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				"Invalid root signature entry");

			if (rootParam)
			{
				SEAssert(rootParam->m_type == RootSignature::RootParameter::Type::DescriptorTable,
					"We currently assume all textures belong to descriptor tables");

				core::InvPtr<re::Texture> const& texture = texSamplerInput.m_texture;

				D3D12_RESOURCE_STATES toState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				D3D12_CPU_DESCRIPTOR_HANDLE descriptor{};

				switch (rootParam->m_tableEntry.m_type)
				{
				case dx12::RootSignature::DescriptorType::SRV:
				{
					if (m_d3dType != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE)
					{
						toState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
					}

					descriptor = dx12::Texture::GetSRV(texture, texSamplerInput.m_textureView);
				}
				break;
				case dx12::RootSignature::DescriptorType::UAV:
				{
					descriptor = dx12::Texture::GetUAV(texture, texSamplerInput.m_textureView);
				}
				break;
				default: SEAssertF("Invalid descriptor range type for a texture");
				}

				// If the depth target is read-only, and we've also used it as an input to a stage, we skip the resource
				// transition (it's handled when binding the depth target as read only)
				const bool skipTransition = (texIdx == depthTargetTexInputIdx);
				if (!skipTransition)
				{
					dx12::Texture::PlatformParams const* texPlatParams =
						texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

					resourceTransitions.emplace_back(TransitionMetadata{
						.m_resource = texPlatParams->m_gpuResource->Get(),
						.m_toState = toState,
						.m_subresourceIndexes = re::TextureView::GetSubresourceIndexes(
							texture, texSamplerInput.m_textureView)
						});
				}

				m_gpuCbvSrvUavDescriptorHeap->SetDescriptorTableEntry(
					rootParam->m_index,
					descriptor,
					rootParam->m_tableEntry.m_offset,
					1);
			}
		}

		// Finally, submit all of our resource transitions in a single batch
		TransitionResourcesInternal(std::move(resourceTransitions));
	}


	void CommandList::SetTextures(
		std::vector<re::TextureAndSamplerInput> const& texSamplerInputs, re::ShaderBindingTable const& sbt)
	{
		dx12::ShaderBindingTable::SetTexturesOnLocalRoots(
			sbt,
			texSamplerInputs,
			this,
			m_gpuCbvSrvUavDescriptorHeap.get(),
			re::RenderManager::Get()->GetCurrentRenderFrameNum());
	}


	void CommandList::TransitionResourceInternal(
		ID3D12Resource* resource,
		D3D12_RESOURCE_STATES toState,
		std::vector<uint32_t>&& subresourceIndexes)
	{
		TransitionResourcesInternal({ TransitionMetadata{
			.m_resource = resource, 
			.m_toState = toState, 
			.m_subresourceIndexes = std::move(subresourceIndexes)} });
	}


	void CommandList::TransitionResourcesInternal(std::vector<TransitionMetadata>&& transitions)
	{
		if (transitions.empty())
		{
			return;
		}


		// Track the D3D resources we've seen during this call, to help us decide whether to insert UAV barriers or not
		std::unordered_set<ID3D12Resource const*> seenResources;
		seenResources.reserve(transitions.size());


		// Batch all barriers into a single call:
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		barriers.reserve(transitions.size() * 12); // Estimate all mips for a 4K texture
		for (auto const& transition : transitions)
		{
			SEAssert(!transition.m_subresourceIndexes.empty(), "Subresources vector is empty");

			SEAssert((transition.m_subresourceIndexes.size() == 1 &&
				transition.m_subresourceIndexes[0] == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) ||
				std::find(transition.m_subresourceIndexes.begin(), transition.m_subresourceIndexes.end(),
					D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) == transition.m_subresourceIndexes.end(),
				"Found an ALL transition in the vector of subresource indexes");

			SEAssert(GetNumSubresources(transition.m_resource, m_device) > 1 ||
				(transition.m_subresourceIndexes.size() == 1 &&
					(transition.m_subresourceIndexes[0] == 0 ||
						transition.m_subresourceIndexes[0] == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)),
				"Invalid transition detected for a resource with a single subresource");


			auto AddBarrier = [this, &transition, &barriers](uint32_t subresourceIdx, D3D12_RESOURCE_STATES toState)
				{
					// If we've already seen this resource before, we can record the transition now (as we prepend any
					// initial transitions when submitting the command list)	
					if (m_resourceStates.HasResourceState(transition.m_resource, subresourceIdx)) // Is the subresource idx (or ALL) in our known states list?
					{
						const D3D12_RESOURCE_STATES currentKnownState = 
							m_resourceStates.GetResourceState(transition.m_resource, subresourceIdx);

#if defined(DEBUG_CMD_LIST_RESOURCE_TRANSITIONS)
						DebugResourceTransitions(
							*this, dx12::GetDebugName(resource).c_str(), currentKnownState, toState, subresourceIdx);
#endif

						if (currentKnownState == toState)
						{
							return; // Before and after states must be different
						}

						barriers.emplace_back(D3D12_RESOURCE_BARRIER{
							.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
							.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
							.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
								.pResource = transition.m_resource,
								.Subresource = subresourceIdx,
								.StateBefore = currentKnownState,
								.StateAfter = toState}
							});
					}
#if defined(DEBUG_CMD_LIST_RESOURCE_TRANSITIONS)
					else
					{
						DebugResourceTransitions(
							*this, dx12::GetDebugName(resource).c_str(), toState, toState, subresourceIdx, true); // PENDING
					}
#endif

					// Record the pending state if necessary, and new state after the transition:
					m_resourceStates.SetResourceState(transition.m_resource, toState, subresourceIdx);
				};


			// We're transitioning to a UAV state, we may need a UAV barrier. We try and skip this when possible (i.e.
			// don't add barriers if we haven't seen the resource in a UAV state before this call)
			if (transition.m_toState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS &&
				!seenResources.contains(transition.m_resource) && // Ignore resources already seen
				m_resourceStates.HasSeenSubresourceInState(transition.m_resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
			{
				// We've accessed this resource before on this command list, and it was transitioned to a UAV state at
				// some point before this call. We must ensure any previous work was done before we access it again
				InsertUAVBarrier(transition.m_resource);
			}
			seenResources.emplace(transition.m_resource);

			// Per-subresource transitions:
			for (uint32_t subresourceIdx : transition.m_subresourceIndexes)
			{
				// Transition the appropriate subresources:
				if (subresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES ||
					GetNumSubresources(transition.m_resource, m_device) == 1) // Bug fix: Force all single subresources to use ALL
				{
					// We can only transition ALL subresources in a single barrier if the before state is the same for all
					// subresources. If we have any pending transitions for individual subresources, this is not the case:
					// We must transition each pending subresource individually to ensure all subresources have the correct
					// before and after state.

					// We need to transition 1-by-1 if there are individual pending subresource states, and we've got an ALL
					// transition
					bool doTransitionAllSubresources = true;
					if (m_resourceStates.GetPendingResourceStates().contains(transition.m_resource))
					{
						auto const& pendingResourceStates = 
							m_resourceStates.GetPendingResourceStates().at(transition.m_resource);

						const bool hasPendingAllSubresourcesRecord =
							pendingResourceStates.HasSubresourceRecord(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

						const size_t numPendingTransitions = pendingResourceStates.GetStates().size();

						const bool hasIndividualPendingSubresourceTransitions =
							(!hasPendingAllSubresourcesRecord && numPendingTransitions > 0) ||
							(hasPendingAllSubresourcesRecord && numPendingTransitions > 1);

						if (hasIndividualPendingSubresourceTransitions)
						{
							doTransitionAllSubresources = false;

							auto const& pendingStates = pendingResourceStates.GetStates();
							for (auto const& pendingState : pendingStates)
							{
								if (pendingState.first == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
								{
									continue;
								}

								const uint32_t pendingSubresourceIdx = pendingState.first;

								AddBarrier(pendingSubresourceIdx, transition.m_toState);
							}
						}
					}

					// We didn't need to process our transitions one-by-one: Submit a single ALL transition:
					if (doTransitionAllSubresources)
					{
						AddBarrier(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, transition.m_toState);
					}
				}
				else
				{
					AddBarrier(subresourceIdx, transition.m_toState);
				}
			}
		}
		
		if (!barriers.empty()) // Might not have recored a barrier if it's the 1st time we've seen a resource
		{
			// Submit all of our transitions in a single batch
			ResourceBarrier(util::CheckedCast<uint32_t>(barriers.size()), barriers.data());
		}
	}


	void CommandList::TransitionResource(
		core::InvPtr<re::Texture> const& texture, D3D12_RESOURCE_STATES toState, re::TextureView const& texView)
	{
		dx12::Texture::PlatformParams const* texPlatParams =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		TransitionResources({ TransitionMetadata {
			.m_resource = texPlatParams->m_gpuResource->Get(),
			.m_toState = toState,
			.m_subresourceIndexes = re::TextureView::GetSubresourceIndexes(texture, texView)
			} });
	}


	void CommandList::InsertUAVBarrier(ID3D12Resource* resource)
	{
		/*
		* Note: This barrier should be used in the scenario where 2 subsequent compute dispatches executed on the same
		* command list access the same UAV, and the second dispatch needs to wait for the first to finish.
		* UAV barriers are intended to ensure write ordering. They're NOT needed:
		* - between 2 draw/dispatch calls that only read a UAV
		* - between 2 draw/dispatch calls that write to a UAV IFF the writes can be executed in any order
		* https://asawicki.info/news_1722_secrets_of_direct3d_12_copies_to_the_same_buffer
		*
		* This function should only be called when we know we definitely need this barrier inserted.
		*/

		const D3D12_RESOURCE_BARRIER barrier{
			.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
			.Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE,
			.UAV = D3D12_RESOURCE_UAV_BARRIER{
				.pResource = resource}
		};

		// TODO: Support batching of multiple barriers
		ResourceBarrier(1, &barrier);
	}


	void CommandList::InsertUAVBarrier(core::InvPtr<re::Texture> const& texture)
	{
		InsertUAVBarrier(texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>()->m_gpuResource->Get());
	}


	void CommandList::ResourceBarrier(uint32_t numBarriers, D3D12_RESOURCE_BARRIER const* barriers)
	{
		SEAssert(numBarriers > 0, "Attempting to submit 0 barriers");

		m_commandList->ResourceBarrier(numBarriers, barriers);
	}


	LocalResourceStateTracker const& CommandList::GetLocalResourceStates() const
	{
		return m_resourceStates;
	}


	void CommandList::DebugPrintResourceStates() const
	{
		LOG("\n------------------------------------\n"
			"\tCommandList \"%s\"\n"
			"\t------------------------------------", 
			GetDebugName(m_commandList.Get()).c_str());
		m_resourceStates.DebugPrintResourceStates();
	}
}