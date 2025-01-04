// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "Buffer.h"
#include "Buffer_DX12.h"
#include "Context_DX12.h"
#include "CommandList_DX12.h"
#include "Debug_DX12.h"
#include "MeshPrimitive.h"
#include "PipelineState_DX12.h"
#include "RenderManager.h"
#include "RootSignature_DX12.h"
#include "SwapChain_DX12.h"
#include "SysInfo_DX12.h"
#include "Texture.h"
#include "Texture_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_DX12.h"
#include "TextureView.h"

#include "Core/Config.h"

#include "Core/Util/CastUtils.h"
#include "Core/Util/MathUtils.h"

#include <d3dx12.h>

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
			dx12::GetDebugName(cmdList.GetD3DCommandList()).c_str(),
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
		ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type, std::wstring const& name)
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


	CommandList::CommandList(ID3D12Device2* device, CommandListType type)
		: m_commandList(nullptr)
		, m_type(type)
		, m_d3dType(TranslateToD3DCommandListType(type))
		, m_commandAllocator(nullptr)
		, m_commandAllocatorReuseFenceValue(0)
		, k_commandListNumber(s_commandListNumber++)
		, m_gpuCbvSrvUavDescriptorHeaps(nullptr)
		, m_currentRootSignature(nullptr)
		, m_currentPSO(nullptr)
	{
		SEAssert(device, "Device cannot be null");

		// Name the command list with a monotonically-increasing index to make it easier to identify
		const std::wstring commandListname = std::wstring(
			GetCommandListTypeWName(type)) +
			L"_CommandList_#" + std::to_wstring(k_commandListNumber);

		m_commandAllocator = CreateCommandAllocator(device, m_d3dType, commandListname + L"_CommandAllocator");

		// Create the command list:
		HRESULT hr = device->CreateCommandList(
			dx12::SysInfo::GetDeviceNodeMask(),
			m_d3dType,							// Direct draw/compute/copy/etc
			m_commandAllocator.Get(),		// The command allocator the command lists will be created on
			nullptr,						// Optional: Command list initial pipeline state
			IID_PPV_ARGS(&m_commandList));	// IID_PPV_ARGS: RIID & destination for the populated command list
		CheckHResult(hr, "Failed to create command list");

		m_commandList->SetName(commandListname.c_str());

		// Set the descriptor heaps (unless we're a copy command list):
		if (m_d3dType != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY)
		{
			// Create our GPU-visible descriptor heaps:
			m_gpuCbvSrvUavDescriptorHeaps = std::make_unique<GPUDescriptorHeap>(m_type, m_commandList.Get());
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
		m_gpuCbvSrvUavDescriptorHeaps = nullptr;
		m_currentRootSignature = nullptr;
		m_currentPSO = nullptr;
	}


	void CommandList::Reset()
	{
		m_currentRootSignature = nullptr;
		m_currentPSO = nullptr;

		// Reset the command allocator BEFORE we reset the command list (to avoid leaking memory)
		m_commandAllocator->Reset();

		m_resourceStates.Reset();

		// Note: pso is optional here; nullptr sets a dummy PSO
		HRESULT hr = m_commandList->Reset(m_commandAllocator.Get(), nullptr);
		CheckHResult(hr, "Failed to reset command list");

		// Re-bind the descriptor heaps (unless we're a copy command list):
		if (m_d3dType != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY)
		{
			// Reset the GPU descriptor heap managers:
			m_gpuCbvSrvUavDescriptorHeaps->Reset();

			ID3D12DescriptorHeap* descriptorHeap = m_gpuCbvSrvUavDescriptorHeaps->GetD3DDescriptorHeap();
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

		m_gpuCbvSrvUavDescriptorHeaps->ParseRootSignatureDescriptorTables(rootSig);

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

		m_gpuCbvSrvUavDescriptorHeaps->ParseRootSignatureDescriptorTables(rootSig);

		ID3D12RootSignature* rootSignature = rootSig->GetD3DRootSignature();
		SEAssert(rootSignature, "Root signature is null. This is unexpected");

		m_commandList->SetComputeRootSignature(rootSignature);
	}


	void CommandList::SetBuffer(re::BufferInput const& bufferInput)
	{
		SEAssert(m_currentRootSignature != nullptr, "Root signature has not been set");
		SEAssert(m_type == CommandListType::Direct || m_type == CommandListType::Compute,
			"Unexpected command list type for setting a buffer on");

		re::Buffer const* buffer = bufferInput.GetBuffer();
		dx12::Buffer::PlatformParams* bufferPlatParams = 
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		RootSignature::RootParameter const* rootSigEntry = 
			m_currentRootSignature->GetRootSignatureEntry(bufferInput.GetShaderName());
		SEAssert(rootSigEntry ||
			core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
			"Invalid root signature entry");

		if (rootSigEntry)
		{
			const uint32_t rootSigIdx = rootSigEntry->m_index;

			bool transitionResource = false;
			D3D12_RESOURCE_STATES toState = D3D12_RESOURCE_STATE_COMMON; // Updated below

			re::Buffer::BufferParams const& bufferParams = buffer->GetBufferParams();

			// Don't transition resources representing shared heaps
			const bool isInSharedHeap = bufferParams.m_lifetime == re::Lifetime::SingleFrame;

			switch (rootSigEntry->m_type)
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
				SEAssert(rootSigEntry->m_type == RootSignature::RootParameter::Type::CBV,
					"Unexpected root signature type");
				SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams) &&
					!re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams),
					"Invalid usage flags for a constant buffer");

				m_gpuCbvSrvUavDescriptorHeaps->SetInlineCBV(
					rootSigIdx,
					bufferPlatParams->m_resovedGPUResource,
					bufferPlatParams->m_heapByteOffset);

				toState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
				transitionResource = !isInSharedHeap;
			}
			break;
			case RootSignature::RootParameter::Type::SRV:
			{
				SEAssert(re::Buffer::HasUsageBit(re::Buffer::Structured, bufferParams),
					"Buffer is missing the Structured usage bit");
				SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams),
					"SRV buffers must have GPU reads enabled");

				m_gpuCbvSrvUavDescriptorHeaps->SetInlineSRV(
					rootSigIdx,
					bufferPlatParams->m_resovedGPUResource,
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

				m_gpuCbvSrvUavDescriptorHeaps->SetInlineUAV(
					rootSigIdx,
					bufferPlatParams->m_resovedGPUResource,
					bufferPlatParams->m_heapByteOffset);

				if (re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams))
				{
					// TODO: We should only insert a UAV barrier if the we're accessing the resource on the same
					// command list where a prior modifying use was performed
					InsertUAVBarrier(bufferPlatParams->m_resovedGPUResource);
				}

				toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
				transitionResource = true;
			}
			break;
			case RootSignature::RootParameter::Type::DescriptorTable:
			{
				switch (rootSigEntry->m_tableEntry.m_type)
				{
				case dx12::RootSignature::DescriptorType::SRV:
				{
					SEAssert(re::Buffer::HasUsageBit(re::Buffer::Usage::Structured, bufferParams),
						"Buffer is missing the Structured usage bit");
					SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams),
						"SRV buffers must have GPU reads enabled");
					SEAssert(bufferPlatParams->m_heapByteOffset == 0, "Unexpected heap byte offset");

					re::BufferView const& bufView = bufferInput.GetView();
					
					m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
						rootSigEntry->m_index,
						dx12::Buffer::GetSRV(bufferInput.GetBuffer(), bufView),
						rootSigEntry->m_tableEntry.m_offset + bufView.m_buffer.m_firstDestIdx,
						1);

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

					re::BufferView const& bufView = bufferInput.GetView();

					m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
						rootSigEntry->m_index,
						dx12::Buffer::GetUAV(bufferInput.GetBuffer(), bufferInput.GetView()),
						rootSigEntry->m_tableEntry.m_offset + bufView.m_buffer.m_firstDestIdx,
						1);

					if (re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams))
					{
						// TODO: We should only insert a UAV barrier if the we're accessing the resource on the same
						// command list where a prior modifying use was performed
						InsertUAVBarrier(bufferPlatParams->m_resovedGPUResource);
					}

					toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
					transitionResource = true;
				}
				break;
				case dx12::RootSignature::DescriptorType::CBV:
				{
					SEAssert(re::Buffer::HasUsageBit(re::Buffer::Constant, bufferParams),
						"Buffer is missing the Constant usage bit");
					SEAssert(re::Buffer::HasAccessBit(re::Buffer::GPURead, bufferParams) &&
						!re::Buffer::HasAccessBit(re::Buffer::GPUWrite, bufferParams),
						"Invalid usage flags for a constant buffer");
					
					re::BufferView const& bufView = bufferInput.GetView();

					m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
						rootSigEntry->m_index,
						dx12::Buffer::GetCBV(bufferInput.GetBuffer(), bufView),
						rootSigEntry->m_tableEntry.m_offset + bufView.m_buffer.m_firstDestIdx,
						1);

					toState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
					transitionResource = !isInSharedHeap;
				}
				break;
				default: SEAssertF("Invalid type");
				}
			}
			break;
			default: SEAssertF("Invalid root parameter type");
			}

			if (transitionResource)
			{
				SEAssert(!isInSharedHeap, "Trying to transition a resource in a shared heap. This is unexpected");
				SEAssert(toState != D3D12_RESOURCE_STATE_COMMON, "Unexpected to state")
				TransitionResource(bufferPlatParams->m_resovedGPUResource, toState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			}

			// If our buffer has CPU readback enabled, add it to our tracking list so we can schedule a copy later on:
			if (re::Buffer::HasAccessBit(re::Buffer::CPURead, bufferParams))
			{
				const uint8_t readbackIdx = dx12::RenderManager::GetIntermediateResourceIdx();

				m_seenReadbackResources.emplace_back(ReadbackResourceMetadata{
					.m_srcResource = bufferPlatParams->m_resovedGPUResource,
					.m_dstResource = bufferPlatParams->m_readbackResources[readbackIdx].m_readbackGPUResource->Get(),
					.m_dstModificationFence = &bufferPlatParams->m_readbackResources[readbackIdx].m_readbackFence,
					.m_dstModificationFenceMutex = &bufferPlatParams->m_readbackResources[readbackIdx].m_readbackFenceMutex });
			}
		}
	}


	void CommandList::Dispatch(glm::uvec3 const& numThreads)
	{
		SEAssert(numThreads.x < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION &&
			numThreads.y < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION &&
			numThreads.z < D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION,
			"Invalid dispatch dimensions");

		CommitGPUDescriptors();

		m_commandList->Dispatch(numThreads.x, numThreads.y, numThreads.z);
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
				*streamBufferPlatParams->GetOrCreateVertexBufferView(*streamBuffer, vertexBuffers[streamIdx].m_view));

			// Currently, single-frame buffers are held in a shared heap so we can't/don't need to transition them here
			if (streamBuffer->GetLifetime() != re::Lifetime::SingleFrame)
			{
				TransitionResource(
					streamBufferPlatParams->m_resovedGPUResource,
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
					D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			}

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
			streamBufferPlatParams->GetOrCreateIndexBufferView(*indexBuffer.GetBuffer(), indexBuffer.m_view));

		// Currently, single-frame buffers are held in a shared heap so we can't/don't need to transition them here
		if (indexBuffer.GetBuffer()->GetLifetime() != re::Lifetime::SingleFrame)
		{
			TransitionResource(
				streamBufferPlatParams->m_resovedGPUResource,
				D3D12_RESOURCE_STATE_INDEX_BUFFER,
				D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
	}


	void CommandList::ClearDepthTarget(re::TextureTarget const& depthTarget)
	{
		if (depthTarget.GetClearMode() == re::TextureTarget::ClearMode::Enabled)
		{
			core::InvPtr<re::Texture> const& depthTex = depthTarget.GetTexture();

			re::Texture::TextureParams const& depthTexParams = depthTex->GetTextureParams();

			SEAssert((depthTexParams.m_usage & re::Texture::Usage::DepthTarget) != 0,
				"Target texture must be a depth target");

			SEAssert(depthTex->GetNumMips() == 1, "Depth target has mips. This is (currently) unexpected");

			re::TextureTarget::TargetParams const& depthTargetParams = depthTarget.GetTargetParams();

			SEAssert(depthTargetParams.m_textureView.DepthWritesEnabled(), "Texture view has depth writes disabled");

			// Ensure we're in a depth write state:
			TransitionResource(depthTex, D3D12_RESOURCE_STATE_DEPTH_WRITE, depthTargetParams.m_textureView);

			const D3D12_CPU_DESCRIPTOR_HANDLE targetDescriptor = 
				dx12::Texture::GetDSV(depthTex, depthTargetParams.m_textureView);

			m_commandList->ClearDepthStencilView(
				targetDescriptor,
				D3D12_CLEAR_FLAG_DEPTH,
				depthTexParams.m_clear.m_depthStencil.m_depth,
				depthTexParams.m_clear.m_depthStencil.m_stencil,
				0,
				nullptr);
		}
	}


	void CommandList::ClearColorTarget(re::TextureTarget const* colorTarget)
	{
		SEAssert(colorTarget, "Target texture cannot be null");

		SEAssert((colorTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::ColorTarget) ||
			(colorTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::SwapchainColorProxy),
			"Target texture must be a color target");

		if (colorTarget->GetClearMode() == re::TextureTarget::ClearMode::Enabled)
		{
			core::InvPtr<re::Texture> const& colorTargetTex = colorTarget->GetTexture();

			re::TextureTarget::TargetParams const& colorTargetParams = colorTarget->GetTargetParams();

			const uint32_t subresourceIdx = 
				re::TextureView::GetSubresourceIndex(colorTargetTex, colorTargetParams.m_textureView);

			TransitionResource(
				colorTargetTex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>()->m_gpuResource->Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, 
				subresourceIdx);

			const D3D12_CPU_DESCRIPTOR_HANDLE targetDescriptor = 
				dx12::Texture::GetRTV(colorTargetTex, colorTargetParams.m_textureView);

			m_commandList->ClearRenderTargetView(
				targetDescriptor,
				&colorTargetTex->GetTextureParams().m_clear.m_color.r,
				0,			// Number of rectangles in the proceeding D3D12_RECT ptr
				nullptr);	// Ptr to an array of rectangles to clear in the resource view. Clears entire view if null
		}
	}


	void CommandList::ClearColorTargets(re::TextureTargetSet const& targetSet)
	{
		for (re::TextureTarget const& target : targetSet.GetColorTargets())
		{
			if (!target.HasTexture())
			{
				break;
			}
			ClearColorTarget(&target);
		}
	}


	void CommandList::ClearTargets(re::TextureTargetSet const& targetSet)
	{
		ClearColorTargets(targetSet);
		if (targetSet.HasDepthTarget())
		{
			ClearDepthTarget(targetSet.GetDepthStencilTarget());
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

		// Track the D3D resources we've seen during this call, to help us decide whether to insert a UAV barrier or not
		std::unordered_set<ID3D12Resource const*> seenResources;
		seenResources.reserve(rwTexInputs.size());
		auto ResourceWasTransitionedInThisCall = [&seenResources](ID3D12Resource const* newResource) -> bool
			{
				return seenResources.contains(newResource);
			};

		for (size_t i = 0; i < rwTexInputs.size(); i++)
		{
			re::RWTextureInput const& rwTexInput = rwTexInputs[i];

			RootSignature::RootParameter const* rootSigEntry =
				m_currentRootSignature->GetRootSignatureEntry(rwTexInput.m_shaderName);

			SEAssert(rootSigEntry ||
				core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
				"Invalid root signature entry");

			if (rootSigEntry)
			{
				SEAssert(rootSigEntry->m_type == RootSignature::RootParameter::Type::DescriptorTable,
					"We currently assume all textures belong to descriptor tables");

				SEAssert(rootSigEntry->m_tableEntry.m_type == dx12::RootSignature::DescriptorType::UAV,
					"RW textures must be UAVs");

				core::InvPtr<re::Texture> const& rwTex = rwTexInput.m_texture;

				SEAssert(((rwTex->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget) == 0) &&
					((rwTex->GetTextureParams().m_usage & re::Texture::Usage::ColorTarget) != 0),
					"Unexpected texture usage for a RW texture");

				m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
					rootSigEntry->m_index,
					dx12::Texture::GetUAV(rwTex, rwTexInput.m_textureView),
					rootSigEntry->m_tableEntry.m_offset,
					1);

				dx12::Texture::PlatformParams const* texPlatParams =
					rwTex->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

				// We're writing to a UAV, we may need a UAV barrier:
				ID3D12Resource* resource = texPlatParams->m_gpuResource->Get();
				if (m_resourceStates.HasSeenSubresourceInState(resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS) &&
					!ResourceWasTransitionedInThisCall(resource))
				{
					// We've accessed this resource before on this command list, and it was transitioned to a UAV
					// state at some point before this call to SetRWTextures. We must ensure any previous work was
					// done before we access it again
					// TODO: This could/should be handled on a per-subresource level. Currently, this results in UAV
					// barriers even when it's a different subresource that was used in a UAV operation
					InsertUAVBarrier(rwTex);
				}
				seenResources.emplace(resource);

				// Insert our resource transition:
				TransitionResource(
					rwTex,
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					rwTexInput.m_textureView);
			}
		}

		// TODO: Support compute target clearing (tricky: Need a copy of descriptors in the GPU-visible heap)
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

		TransitionResource(
			bufferPlatformParams->m_resovedGPUResource,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		m_commandList->CopyBufferRegion(
			bufferPlatformParams->m_resovedGPUResource,	// pDstBuffer
			dstOffset,									// DstOffset
			srcResource,								// pSrcBuffer
			srcOffset,									// SrcOffset
			numBytes);									// NumBytes
	}


	void CommandList::CopyResource(ID3D12Resource* srcResource, ID3D12Resource* dstResource)
	{
		TransitionResource(srcResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		TransitionResource(dstResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		m_commandList->CopyResource(dstResource, srcResource);
	}


	void CommandList::SetTexture(re::TextureAndSamplerInput const& texSamplerInput, bool skipTransition)
	{
		SEAssert(m_currentPSO, "Pipeline is not currently set");

		core::InvPtr<re::Texture> const& texture = texSamplerInput.m_texture;

		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		dx12::Texture::PlatformParams const* texPlatParams =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		RootSignature::RootParameter const* rootSigEntry =
			m_currentRootSignature->GetRootSignatureEntry(texSamplerInput.m_shaderName);
		SEAssert(rootSigEntry ||
			core::Config::Get()->KeyExists(core::configkeys::k_strictShaderBindingCmdLineArg) == false,
			"Invalid root signature entry");

		if (rootSigEntry)
		{
			SEAssert(rootSigEntry->m_type == RootSignature::RootParameter::Type::DescriptorTable,
				"We currently assume all textures belong to descriptor tables");

			D3D12_RESOURCE_STATES toState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			D3D12_CPU_DESCRIPTOR_HANDLE descriptor{};

			switch (rootSigEntry->m_tableEntry.m_type)
			{
			case dx12::RootSignature::DescriptorType::SRV:
			{
				SEAssert(m_d3dType == D3D12_COMMAND_LIST_TYPE_COMPUTE || m_d3dType == D3D12_COMMAND_LIST_TYPE_DIRECT,
					"Unexpected command list type");

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

				SEAssertF("TODO: Support/test setting UAVs as a texture input (need to handle UAV barriers?)");
			}
			break;
			default:
				SEAssertF("Invalid range type");
			}

			// If a depth resource is used as both an input and target, we'll record the (read only) transition when
			// setting the target
			if (!skipTransition)
			{
				TransitionResource(texture, toState, texSamplerInput.m_textureView);
			}

			m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
				rootSigEntry->m_index,
				descriptor,
				rootSigEntry->m_tableEntry.m_offset,
				1);
		}
	}


	void CommandList::TransitionResourceInternal(
		ID3D12Resource* resource,
		D3D12_RESOURCE_STATES toState,
		std::vector<uint32_t> subresourceIndexes)
	{
		SEAssert(!subresourceIndexes.empty(), "Subresources vector is empty");

		SEAssert((subresourceIndexes.size() == 1 && 
				subresourceIndexes[0] == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) ||
			std::find(subresourceIndexes.begin(), subresourceIndexes.end(), 
				D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) == subresourceIndexes.end(),
			"Found an ALL transition in the vector of subresource indexes");

		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		barriers.reserve(subresourceIndexes.size());

		auto AddBarrier = [this, &resource, &barriers](uint32_t subresourceIdx, D3D12_RESOURCE_STATES toState)
			{
				// If we've already seen this resource before, we can record the transition now (as we prepend any initial
				// transitions when submitting the command list)	
				if (m_resourceStates.HasResourceState(resource, subresourceIdx)) // Is the subresource idx (or ALL) in our known states list?
				{
					const D3D12_RESOURCE_STATES currentKnownState = m_resourceStates.GetResourceState(resource, subresourceIdx);

#if defined(DEBUG_CMD_LIST_RESOURCE_TRANSITIONS)
					DebugResourceTransitions(
						*this, dx12::GetDebugName(resource).c_str(), currentKnownState, toState, subresourceIdx);
#endif

					if (currentKnownState == toState)
					{
						return; // Before and after states must be different
					}

					barriers.emplace_back(D3D12_RESOURCE_BARRIER{
						.Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
						.Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE,
						.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
							.pResource = resource,
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
				m_resourceStates.SetResourceState(resource, toState, subresourceIdx);
			};

		for (uint32_t subresourceIdx : subresourceIndexes)
		{
			// Transition the appropriate subresources:
			if (subresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
			{
				// We can only transition ALL subresources in a single barrier if the before state is the same for all
				// subresources. If we have any pending transitions for individual subresources, this is not the case:
				// We must transition each pending subresource individually to ensure all subresources have the correct
				// before and after state.

				// We need to transition 1-by-1 if there are individual pending subresource states, and we've got an ALL
				// transition
				bool doTransitionAllSubresources = true;
				if (m_resourceStates.GetPendingResourceStates().contains(resource))
				{
					auto const& pendingResourceStates = m_resourceStates.GetPendingResourceStates().at(resource);
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

							AddBarrier(pendingSubresourceIdx, toState);
						}
					}
				}

				// We didn't need to process our transitions one-by-one: Submit a single ALL transition:
				if (doTransitionAllSubresources)
				{
					AddBarrier(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, toState);
				}
			}
			else
			{
				AddBarrier(subresourceIdx, toState);
			}
		}

		if (!barriers.empty()) // Might not have recored a barrier if it's the 1st time we've seen a resource
		{
			// Submit all of our transitions in a single batch
			ResourceBarrier(util::CheckedCast<uint32_t>(barriers.size()), barriers.data());
		}
	}


	void CommandList::TransitionResource(
		ID3D12Resource* resource, D3D12_RESOURCE_STATES toState, uint32_t subresourceIdx)
	{
		TransitionResourceInternal(resource, toState, { subresourceIdx });
	}


	void CommandList::TransitionResource(
		core::InvPtr<re::Texture> const& texture, D3D12_RESOURCE_STATES toState, re::TextureView const& texView)
	{
		dx12::Texture::PlatformParams const* texPlatParams =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		TransitionResourceInternal(
			texPlatParams->m_gpuResource->Get(),
			toState,
			re::TextureView::GetSubresourceIndexes(texture, texView));
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