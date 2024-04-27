// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "Buffer.h"
#include "Buffer_DX12.h"
#include "Core\Util\CastUtils.h"
#include "Config.h"
#include "Context_DX12.h"
#include "CommandList_DX12.h"
#include "Debug_DX12.h"
#include "Core\Util\MathUtils.h"
#include "MeshPrimitive.h"
#include "RenderManager.h"
#include "RootSignature_DX12.h"
#include "SwapChain_DX12.h"
#include "SysInfo_DX12.h"
#include "Texture.h"
#include "Texture_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_DX12.h"
#include "VertexStream.h"
#include "VertexStream_DX12.h"

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


	constexpr D3D_PRIMITIVE_TOPOLOGY TranslateToD3DPrimitiveTopology(gr::MeshPrimitive::TopologyMode topologyMode)
	{
		switch (topologyMode)
		{
		case gr::MeshPrimitive::TopologyMode::PointList: return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		case gr::MeshPrimitive::TopologyMode::LineList: return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case gr::MeshPrimitive::TopologyMode::LineStrip: return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case gr::MeshPrimitive::TopologyMode::TriangleList: return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case gr::MeshPrimitive::TopologyMode::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case gr::MeshPrimitive::TopologyMode::LineListAdjacency: return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
		case gr::MeshPrimitive::TopologyMode::LineStripAdjacency: return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
		case gr::MeshPrimitive::TopologyMode::TriangleListAdjacency: return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
		case gr::MeshPrimitive::TopologyMode::TriangleStripAdjacency: return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
		default:
			SEAssertF("Invalid topology mode");
			return D3D_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}

	// As per the RWTexture2D<float4> outputs defined in SaberComputeCommon.hlsli
	constexpr char const* k_uavTexTargetNames[] =
	{
		"output0",
		"output1",
		"output2",
		"output3",
		"output4",
		"output5",
		"output6",
		"output7",
	};
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

			constexpr uint8_t k_numHeaps = 1;
			ID3D12DescriptorHeap* descriptorHeaps[k_numHeaps] = {
				m_gpuCbvSrvUavDescriptorHeaps->GetD3DDescriptorHeap()
			};
			m_commandList->SetDescriptorHeaps(k_numHeaps, descriptorHeaps);
		}

		m_commandAllocatorReuseFenceValue = 0;

		m_seenReadbackResources.clear();
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


	void CommandList::SetBuffer(re::Buffer const* buffer)
	{
		SEAssert(m_currentRootSignature != nullptr, "Root signature has not been set");
		SEAssert(m_type == CommandListType::Direct || m_type == CommandListType::Compute,
			"Unexpected command list type for setting a buffer on");

		dx12::Buffer::PlatformParams* bufferPlatParams =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams*>();

		RootSignature::RootParameter const* rootSigEntry = 
			m_currentRootSignature->GetRootSignatureEntry(buffer->GetName());
		SEAssert(rootSigEntry ||
			en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false,
			"Invalid root signature entry");

		if (rootSigEntry)
		{
			const uint32_t rootSigIdx = rootSigEntry->m_index;

			bool transitionResource = false;
			D3D12_RESOURCE_STATES toState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

			switch (buffer->GetBufferParams().m_dataType)
			{
			case re::Buffer::DataType::Constant:
			{
				SEAssert(rootSigEntry->m_type == RootSignature::RootParameter::Type::CBV,
					"Unexpected root signature type");

				SEAssert((buffer->GetBufferParams().m_usageMask & re::Buffer::Usage::GPURead) != 0 &&
					(buffer->GetBufferParams().m_usageMask & re::Buffer::Usage::GPUWrite) == 0,
					"Invalid usage flags for a constant buffer");

				m_gpuCbvSrvUavDescriptorHeaps->SetInlineCBV(
					rootSigIdx,
					bufferPlatParams->m_resource.Get(),
					bufferPlatParams->m_heapByteOffset);

				transitionResource = false;
			}
			break;
			case re::Buffer::DataType::Structured:
			{
				switch (rootSigEntry->m_type)
				{
				case RootSignature::RootParameter::Type::SRV:
				{
					SEAssert((buffer->GetBufferParams().m_usageMask & re::Buffer::Usage::GPURead) != 0, 
						"Buffer does not have the GPU read flag set");

					m_gpuCbvSrvUavDescriptorHeaps->SetInlineSRV(
						rootSigIdx,
						bufferPlatParams->m_resource.Get(),
						bufferPlatParams->m_heapByteOffset);

					transitionResource = false;
				}
				break;
				case RootSignature::RootParameter::Type::UAV:
				{
					SEAssert(buffer->GetBufferParams().m_usageMask & re::Buffer::Usage::GPUWrite, 
						"UAV buffers must have GPU writes enabled");

					m_gpuCbvSrvUavDescriptorHeaps->SetInlineUAV(
						rootSigIdx,
						bufferPlatParams->m_resource.Get(),
						bufferPlatParams->m_heapByteOffset);

					if ((buffer->GetBufferParams().m_usageMask & re::Buffer::Usage::GPUWrite))
					{
						InsertUAVBarrier(bufferPlatParams->m_resource.Get());
						toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						transitionResource = true;
					}
					else
					{
						transitionResource = false;
					}
				}
				break;
				case RootSignature::RootParameter::Type::DescriptorTable:
				{
					SEAssert(buffer->GetBufferParams().m_usageMask & re::Buffer::Usage::GPUWrite,
						"UAV buffers must have GPU writes enabled");

					dx12::DescriptorAllocation const& descriptorAllocation = bufferPlatParams->m_uavCPUDescAllocation;

					m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
						rootSigEntry->m_index,
						descriptorAllocation,
						rootSigEntry->m_tableEntry.m_offset,
						1);

					if ((buffer->GetBufferParams().m_usageMask & re::Buffer::Usage::GPUWrite))
					{
						InsertUAVBarrier(bufferPlatParams->m_resource.Get());
						toState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						transitionResource = true;
					}
					else
					{
						transitionResource = false;
					}
				}
				break;
				default: SEAssertF("Invalid or unexpected root signature type");
				}
			}
			break;
			default:
				SEAssertF("Invalid DataType");
			}

			// We only transition GPU-writeable buffers (i.e. immutable with GPU-write flag enabled)
			if (transitionResource)
			{
				TransitionResource(
					bufferPlatParams->m_resource.Get(),
					1,
					toState,
					D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			}

			// If our buffer has CPU readback enabled, add it to our tracking list so we can schedule a copy later on:
			if ((buffer->GetBufferParams().m_usageMask & re::Buffer::Usage::CPURead) != 0)
			{
				const uint8_t readbackIdx = dx12::RenderManager::GetIntermediateResourceIdx();

				m_seenReadbackResources.emplace_back(ReadbackResourceMetadata{
					.m_srcResource = bufferPlatParams->m_resource.Get(),
					.m_dstResource = bufferPlatParams->m_readbackResources[readbackIdx].m_resource.Get(),
					.m_dstModificationFence = &bufferPlatParams->m_readbackResources[readbackIdx].m_readbackFence,
					.m_dstModificationFenceMutex = &bufferPlatParams->m_readbackResources[readbackIdx].m_readbackFenceMutex });
			}
		}
	}


	void CommandList::DrawBatchGeometry(re::Batch const& batch)
	{
		// Set the geometry for the draw:		
		re::Batch::GraphicsParams const& batchGraphicsParams = batch.GetGraphicsParams();

		SetPrimitiveType(TranslateToD3DPrimitiveTopology(batchGraphicsParams.m_batchTopologyMode));

		SetVertexBuffers(
			batchGraphicsParams.m_vertexStreams.data(),
			static_cast<uint8_t>(batchGraphicsParams.m_vertexStreams.size()));

		// Record the draw:
		switch (batchGraphicsParams.m_batchGeometryMode)
		{
		case re::Batch::GeometryMode::IndexedInstanced:
		{
			re::VertexStream const* indexStream = batchGraphicsParams.m_indexStream;
			SEAssert(indexStream, "Index stream cannot be null for indexed draws");

			dx12::VertexStream::PlatformParams_Index* indexPlatformParams =
				indexStream->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Index*>();
			SetIndexBuffer(&indexPlatformParams->m_indexBufferView);

			CommitGPUDescriptors();

			m_commandList->DrawIndexedInstanced(
				indexStream->GetNumElements(),					// Index count, per instance
				static_cast<uint32_t>(batch.GetInstanceCount()),// Instance count
				0,												// Start index location
				0,												// Base vertex location
				0);												// Start instance location
		}
		break;
		case re::Batch::GeometryMode::ArrayInstanced:
		{
			re::VertexStream const* positionStream = batchGraphicsParams.m_vertexStreams[gr::MeshPrimitive::Slot::Position];
			SEAssert(positionStream, "Position stream cannot be null");

			CommitGPUDescriptors();

			m_commandList->DrawInstanced(
				positionStream->GetNumElements(),	// VertexCountPerInstance
				batchGraphicsParams.m_numInstances, // InstanceCount
				0,									// StartVertexLocation
				0);									// StartInstanceLocation
		}
		break;
		default: SEAssertF("Invalid batch geometry type");
		}
	}


	void CommandList::SetVertexBuffer(uint32_t slot, re::VertexStream const* stream)
	{
		dx12::VertexStream::PlatformParams* streamPlatParams =
			stream->GetPlatformParams()->As<dx12::VertexStream::PlatformParams*>();

		TransitionResource(
			streamPlatParams->m_bufferResource.Get(),
			1,
			D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			0);

		m_commandList->IASetVertexBuffers(
			slot, 
			1, 
			&stream->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Vertex*>()->m_vertexBufferView);
	}


	void CommandList::SetVertexBuffers(re::VertexStream const* const* streams, uint8_t count)
	{
		SEAssert(streams && count > 0, "Invalid vertex streams received");

		uint32_t currentStartSlot = 0;

		std::vector<D3D12_VERTEX_BUFFER_VIEW> streamViews;
		streamViews.reserve(count);

		for (uint32_t streamIdx = 0; streamIdx < count; streamIdx++)
		{
			if (streams[streamIdx] == nullptr)
			{
				// Submit the list we've built so far
				if (!streamViews.empty())
				{
					m_commandList->IASetVertexBuffers(
						currentStartSlot, 
						static_cast<uint32_t>(streamViews.size()), 
						&streamViews[0]);

					streamViews.clear();
				}

				// Prepare for the next iteration:
				currentStartSlot = streamIdx + 1;

				continue;
			}

			dx12::VertexStream::PlatformParams* streamPlatParams =
				streams[streamIdx]->GetPlatformParams()->As<dx12::VertexStream::PlatformParams*>();

			TransitionResource(
				streamPlatParams->m_bufferResource.Get(),
				1,
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
				0);

			streamViews.emplace_back(
				streams[streamIdx]->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Vertex*>()->m_vertexBufferView);
		}

		if (!streamViews.empty())
		{
			m_commandList->IASetVertexBuffers(
				currentStartSlot, 
				static_cast<uint32_t>(streamViews.size()), 
				&streamViews[0]);
		}
	}


	void CommandList::ClearDepthTarget(re::TextureTarget const* depthTarget) const
	{
		SEAssert(depthTarget, "Target texture cannot be null");

		if (depthTarget->GetClearMode() == re::TextureTarget::TargetParams::ClearMode::Enabled)
		{
			std::shared_ptr<re::Texture> depthTex = depthTarget->GetTexture();

			re::Texture::TextureParams const& depthTexParams = depthTex->GetTextureParams();

			SEAssert((depthTexParams.m_usage & re::Texture::Usage::DepthTarget) != 0,
				"Target texture must be a depth target");

			const uint32_t numDepthMips = depthTex->GetNumMips();
			SEAssert(numDepthMips == 1, "Depth target has mips. This is unexpected");

			re::TextureTarget::TargetParams const& depthTargetParams = depthTarget->GetTargetParams();

			dx12::TextureTarget::PlatformParams* depthTargetPlatParams =
				depthTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

			if (depthTargetParams.m_targetFace == re::TextureTarget::k_allFaces)
			{
				SEAssert(depthTexParams.m_dimension == re::Texture::Dimension::TextureCubeMap,
					"We're (currently) expecting the a cubemap");

				D3D12_CPU_DESCRIPTOR_HANDLE const& dsvDescriptor =
					depthTargetPlatParams->m_cubemapDescriptor.GetBaseDescriptor();

				m_commandList->ClearDepthStencilView(
					dsvDescriptor,
					D3D12_CLEAR_FLAG_DEPTH,
					depthTexParams.m_clear.m_depthStencil.m_depth,
					depthTexParams.m_clear.m_depthStencil.m_stencil,
					0,
					nullptr);
			}
			else
			{
				D3D12_CPU_DESCRIPTOR_HANDLE const& dsvDescriptor =
					depthTargetPlatParams->m_rtvDsvDescriptors[depthTarget->GetTargetParams().m_targetFace].GetBaseDescriptor();

				m_commandList->ClearDepthStencilView(
					dsvDescriptor,
					D3D12_CLEAR_FLAG_DEPTH,
					depthTexParams.m_clear.m_depthStencil.m_depth,
					depthTexParams.m_clear.m_depthStencil.m_stencil,
					0,
					nullptr);
			}
		}
	}


	void CommandList::ClearColorTarget(re::TextureTarget const* colorTarget) const
	{
		SEAssert(colorTarget, "Target texture cannot be null");

		SEAssert((colorTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::ColorTarget) ||
			(colorTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::SwapchainColorProxy),
			"Target texture must be a color target");

		if (colorTarget->GetClearMode() == re::TextureTarget::TargetParams::ClearMode::Enabled)
		{
			dx12::TextureTarget::PlatformParams* targetPlatParams =
				colorTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

			m_commandList->ClearRenderTargetView(
				targetPlatParams->m_rtvDsvDescriptors[colorTarget->GetTargetParams().m_targetFace].GetBaseDescriptor(),
				&colorTarget->GetTexture()->GetTextureParams().m_clear.m_color.r,
				0,			// Number of rectangles in the proceeding D3D12_RECT ptr
				nullptr);	// Ptr to an array of rectangles to clear in the resource view. Clears entire view if null
		}
	}


	void CommandList::ClearColorTargets(re::TextureTargetSet const& targetSet) const
	{
		for (re::TextureTarget const& target : targetSet.GetColorTargets())
		{
			if (target.HasTexture())
			{
				ClearColorTarget(&target);
			}
		}
	}


	void CommandList::SetRenderTargets(re::TextureTargetSet const& targetSet, bool readOnlyDepth)
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
			std::shared_ptr<re::Texture> targetTexture = target.GetTexture();

			dx12::Texture::PlatformParams* texPlatParams =
				targetTexture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

			re::TextureTarget::TargetParams const& targetParams = target.GetTargetParams();

			TransitionResource(
				targetTexture.get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				targetParams.m_targetMip);

			const uint32_t numMips = targetTexture->GetNumMips();
			const uint32_t subresourceIdx = (targetParams.m_targetFace * numMips) + targetParams.m_targetMip;

			dx12::TextureTarget::PlatformParams* targetPlatParams =
				target.GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

			// Attach the RTV for the target face:
			colorTargetDescriptors.emplace_back(
				targetPlatParams->m_rtvDsvDescriptors[subresourceIdx].GetBaseDescriptor());
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptor{};
		re::TextureTarget const* depthStencilTarget = targetSet.GetDepthStencilTarget();
		if (depthStencilTarget)
		{
			re::TextureTarget::TargetParams const& depthTargetParams = depthStencilTarget->GetTargetParams();

			const bool depthWriteEnabled =
				depthTargetParams.m_channelWriteMode.R == re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled && 
				!readOnlyDepth;

			const D3D12_RESOURCE_STATES depthState = depthWriteEnabled ?
				D3D12_RESOURCE_STATE_DEPTH_WRITE :
				(D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			TransitionResource(depthStencilTarget->GetTexture().get(),
				depthState,
				depthTargetParams.m_targetMip);

			dx12::TextureTarget::PlatformParams* depthTargetPlatParams =
				depthStencilTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();


			if (depthTargetParams.m_targetFace == re::TextureTarget::k_allFaces)
			{
				SEAssert(depthStencilTarget->GetTexture()->GetTextureParams().m_dimension == re::Texture::Dimension::TextureCubeMap,
					"We're (currently) expecting a cubemap");

				if (depthWriteEnabled)
				{
					dsvDescriptor = depthTargetPlatParams->m_cubemapDescriptor.GetBaseDescriptor();
				}
				else
				{
					// TODO: Select a cube DSV that is created with depth writes disabled
					dsvDescriptor = depthTargetPlatParams->m_cubemapDescriptor.GetBaseDescriptor();
				}
			}
			else
			{
				const uint32_t subresourceIdx = depthTargetParams.m_targetFace;

				if (depthWriteEnabled)
				{
					dsvDescriptor = depthTargetPlatParams->m_rtvDsvDescriptors[subresourceIdx].GetBaseDescriptor();
				}
				else
				{
					// TODO: Select a DSV that is created with depth writes disabled
					dsvDescriptor = depthTargetPlatParams->m_rtvDsvDescriptors[subresourceIdx].GetBaseDescriptor();
				}
			}
		}

		const uint32_t numColorTargets = targetSet.GetNumColorTargets();

		// NOTE: isSingleHandleToDescRange == true specifies that the rtvs are contiguous in memory, thus N rtv 
		// descriptors will be found by offsetting from rtvs[0]. Otherwise, it is assumed rtvs is an array of descriptor
		// pointers
		m_commandList->OMSetRenderTargets(
			numColorTargets,
			colorTargetDescriptors.data(),
			false,			// Our render target descriptors (currently) aren't guaranteed to be in a contiguous range
			dsvDescriptor.ptr == 0 ? nullptr : &dsvDescriptor);

		// Set the viewport and scissor rectangles:
		SetViewport(targetSet);
		SetScissorRect(targetSet);

		// Clear the targets:
		if (numColorTargets > 0)
		{
			ClearColorTargets(targetSet);
		}
		
		if (depthStencilTarget)
		{
			ClearDepthTarget(targetSet.GetDepthStencilTarget());
		}
	}


	void CommandList::SetComputeTargets(re::TextureTargetSet const& textureTargetSet)
	{
		SEAssert(textureTargetSet.GetDepthStencilTarget() == nullptr,
			"It is not possible to attach a depth buffer as a target to a compute shader");

		SEAssert(m_type == CommandListType::Compute, "This function should only be called from compute command lists");
		SEAssert(m_currentPSO, "Pipeline is not currently set");

		// Track the D3D resources we've seen during this call, to help us decide whether to insert a UAV barrier or not
		std::unordered_set<ID3D12Resource const*> seenResources;
		seenResources.reserve(textureTargetSet.GetNumColorTargets());
		auto ResourceWasTransitionedInThisCall = [&seenResources](ID3D12Resource const* newResource) -> bool
		{
			return seenResources.contains(newResource);
		};

		std::vector<re::TextureTarget> const& colorTargets = textureTargetSet.GetColorTargets();
		for (size_t i = 0; i < colorTargets.size(); i++)
		{
			re::TextureTarget const& colorTarget = colorTargets[i];
			if (!colorTarget.HasTexture())
			{
				break; // Targets must be bound in monotonically-increasing order from slot 0
			}			
			std::shared_ptr<re::Texture> colorTex = colorTarget.GetTexture();

			SEAssert((colorTex->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget) == 0,
				"It is unexpected that we're trying to attach a texture with DepthTarget usage to a compute shader");

			// We bind by name, but effectively UAVs targets are (currently) bound to slots[0, 7]
			RootSignature::RootParameter const* rootSigEntry = 
				m_currentRootSignature->GetRootSignatureEntry(k_uavTexTargetNames[i]);

			SEAssert(rootSigEntry || en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false,
				"Invalid root signature entry");

			if (rootSigEntry)
			{
				SEAssert(rootSigEntry->m_type == RootSignature::RootParameter::Type::DescriptorTable,
					"We currently assume all textures belong to descriptor tables");

				SEAssert(rootSigEntry->m_tableEntry.m_type == dx12::RootSignature::DescriptorType::UAV,
					"Compute shaders can only write to UAVs");

				re::TextureTarget::TargetParams const& targetParams = colorTarget.GetTargetParams();

				dx12::Texture::PlatformParams* texPlatParams =
					colorTex->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

				const uint32_t targetMip = targetParams.m_targetMip;

				SEAssert(targetMip < texPlatParams->m_uavCpuDescAllocations.size(), "Not enough UAV descriptors");

				dx12::DescriptorAllocation const& descriptorAllocation =
					texPlatParams->m_uavCpuDescAllocations[targetMip];

				m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
					rootSigEntry->m_index,
					descriptorAllocation,
					rootSigEntry->m_tableEntry.m_offset,
					1);

				// We're writing to a UAV, we may need a UAV barrier:
				ID3D12Resource* resource = texPlatParams->m_textureResource.Get();
				if (m_resourceStates.HasSeenSubresourceInState(resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS) &&
					!ResourceWasTransitionedInThisCall(resource))
				{
					// We've accessed this resource before on this command list, and it was transitioned to a UAV
					// state at some point before this call to SetComputeTargets. We must ensure any previous work was
					// done before we access it again
					// TODO: This could/should be handled on a per-subresource level. Currently, this results in UAV
					// barriers even when it's a different subresource that was used in a UAV operation
					InsertUAVBarrier(colorTex);
				}
				seenResources.emplace(resource);

				// Insert our resource transition:
				TransitionResource(
					colorTex.get(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					targetMip);
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


	void CommandList::UpdateSubresources(
		re::Texture const* texture, ID3D12Resource* intermediate, size_t intermediateOffset)
	{
		SEAssert(m_type == dx12::CommandListType::Copy, "Expected a copy command list");

		dx12::Texture::PlatformParams const* texPlatParams = 
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		const uint8_t bytesPerTexel = re::Texture::GetNumBytesPerTexel(texParams.m_format);
		const uint32_t numBytesPerFace = static_cast<uint32_t>(texture->GetTotalBytesPerFace());

		// Populate our subresource data
		// Note: We currently assume we only have data for the first mip of each face
		std::vector<D3D12_SUBRESOURCE_DATA> subresourceData;
		subresourceData.reserve(texParams.m_faces);

		for (uint32_t faceIdx = 0; faceIdx < texParams.m_faces; faceIdx++)
		{
			// Transition to the copy destination state:
			TransitionResource(texture, D3D12_RESOURCE_STATE_COPY_DEST, re::Texture::k_allMips);

			void const* initialData = texture->GetTexelData(faceIdx);
			SEAssert(initialData, "Initial data cannot be null");

			subresourceData.emplace_back(D3D12_SUBRESOURCE_DATA
				{
					.pData = initialData,

					// https://github.com/microsoft/DirectXTex/wiki/ComputePitch
					// Row pitch: The number of bytes in a scanline of pixels: bytes-per-pixel * width-of-image
					// - Can be larger than the number of valid pixels due to alignment padding
					.RowPitch = bytesPerTexel * texParams.m_width,

					// Slice pitch: The number of bytes in each depth slice
					// - 1D/2D images: The total size of the image, including alignment padding
					.SlicePitch = numBytesPerFace
				});
		}

		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/updatesubresources2
		const uint64_t bufferSizeResult = ::UpdateSubresources(
			m_commandList.Get(),							// Command list
			texPlatParams->m_textureResource.Get(),			// Destination resource
			intermediate,									// Intermediate resource
			0,												// Byte offset to the intermediate resource
			0,												// Index of 1st subresource in the resource
			static_cast<uint32_t>(subresourceData.size()),	// Number of subresources in the subresources array
			subresourceData.data());						// Array of subresource data structs
		SEAssert(bufferSizeResult > 0, "UpdateSubresources returned 0 bytes. This is unexpected");
	}


	void CommandList::UpdateSubresources(
		re::VertexStream const* stream, ID3D12Resource* intermediate, size_t intermediateOffset)
	{
		SEAssert(m_type == dx12::CommandListType::Copy, "Expected a copy command list");

		dx12::VertexStream::PlatformParams const* streamPlatformParams =
			stream->GetPlatformParams()->As<dx12::VertexStream::PlatformParams const*>();

		TransitionResource(
			streamPlatformParams->m_bufferResource.Get(),
			1,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		const size_t bufferSize = stream->GetTotalDataByteSize();

		// Populate the subresource:
		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = stream->GetData();
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		const uint64_t bufferSizeResult = ::UpdateSubresources(
			m_commandList.Get(),
			streamPlatformParams->m_bufferResource.Get(),	// Destination resource
			intermediate,									// Intermediate resource
			0,												// Index of 1st subresource in the resource
			0,												// Number of subresources in the resource.
			1,												// Required byte size for the update
			&subresourceData);
		SEAssert(bufferSizeResult > 0, "UpdateSubresources returned 0 bytes. This is unexpected");
	}


	void CommandList::UpdateSubresources(
		re::Buffer const* buffer, uint32_t dstOffset, ID3D12Resource* srcResource, uint32_t srcOffset, uint32_t numBytes)
	{
		SEAssert(m_type == dx12::CommandListType::Copy, "Expected a copy command list");
		SEAssert((buffer->GetBufferParams().m_usageMask & re::Buffer::Usage::GPUWrite) != 0, 
			"GPU writes must be enabled");

		dx12::Buffer::PlatformParams const* bufferPlatformParams =
			buffer->GetPlatformParams()->As<dx12::Buffer::PlatformParams const*>();

		// Note: We only allow Immutable buffers to live on the default heap; They have a single, unshared backing 
		// resource so this transition is safe
		TransitionResource(
			bufferPlatformParams->m_resource.Get(),
			1,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		m_commandList->CopyBufferRegion(
			bufferPlatformParams->m_resource.Get(), // pDstBuffer
			dstOffset,								// DstOffset
			srcResource,							// pSrcBuffer
			srcOffset,								// SrcOffset
			numBytes);								// NumBytes
	}


	void CommandList::CopyResource(ID3D12Resource* srcResource, ID3D12Resource* dstResource)
	{
		TransitionResource(
			srcResource,
			1,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		// Note: The destination resource is (currently) assumed to be a dedicated readback buffer that is always in
		// the D3D12_RESOURCE_STATE_COPY_DEST state. Thus, we don't record a resource transition barrier for it here.

		m_commandList->CopyResource(dstResource, srcResource);
	}


	void CommandList::SetTexture(
		std::string const& shaderName, re::Texture const* texture, uint32_t srcMip, bool skipTransition)
	{
		SEAssert(m_currentPSO, "Pipeline is not currently set");

		SEAssert(srcMip < texture->GetNumMips() ||
			srcMip == re::Texture::k_allMips, 
			"Unexpected mip level");

		dx12::Texture::PlatformParams const* texPlatParams = 
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		RootSignature::RootParameter const* rootSigEntry =
			m_currentRootSignature->GetRootSignatureEntry(shaderName);
		SEAssert(rootSigEntry ||
			en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false,
			"Invalid root signature entry");

		if (rootSigEntry)
		{
			SEAssert(rootSigEntry->m_type == RootSignature::RootParameter::Type::DescriptorTable,
				"We currently assume all textures belong to descriptor tables");

			D3D12_RESOURCE_STATES toState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
			dx12::DescriptorAllocation const* descriptorAllocation = nullptr;
			switch (rootSigEntry->m_tableEntry.m_type)
			{
			case dx12::RootSignature::DescriptorType::SRV:
			{
				SEAssert(m_d3dType == D3D12_COMMAND_LIST_TYPE_COMPUTE || m_d3dType == D3D12_COMMAND_LIST_TYPE_DIRECT,
					"Unexpected command list type");

				toState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				if (m_d3dType != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE)
				{
					toState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				}			

				// Get the appropriate cpu-visible SRV:
				switch (rootSigEntry->m_tableEntry.m_srvViewDimension)
				{
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE1D:
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
				{
					SEAssertF("TODO: Support this dimension");
				}
				break;
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2D:
				{
					descriptorAllocation = &texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::Texture2D];
				}
				break;
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
				{
					switch (texture->GetTextureParams().m_dimension)
					{
					case re::Texture::Dimension::Texture2D:
					{
						descriptorAllocation = &texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::Texture2D];
					}
					break;
					case re::Texture::Dimension::TextureCubeMap:
					{
						descriptorAllocation = &texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::Texture2DArray];
					}
					break;
					default: SEAssertF("Unexpected texture dimension");
					}
				}
				break;
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2DMS:
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURE3D:
				{
					SEAssertF("TODO: Support this dimension");
				}
				break;
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURECUBE:
				{
					descriptorAllocation = &texPlatParams->m_srvCpuDescAllocations[re::Texture::Dimension::TextureCubeMap];
				}
				break;
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
				{
					SEAssertF("TODO: Support this dimension");
				}
				break;
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_UNKNOWN:
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_BUFFER:
				case D3D12_SRV_DIMENSION::D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
				default: SEAssertF("Invalid/unexpected table entry type");
				}				
			}
			break;
			case dx12::RootSignature::DescriptorType::UAV:
			{
				// This is for UAV *inputs*
				SEAssertF("TODO: Implement this. Need to figure out how to specify the appropriate mip level/subresource index");

				//toState = ???;

				// Note: We don't (shouldn't?) need to record a modification fence value to the texture resource here, 
				// since it's being used as an input

				//descriptorAllocation = &texPlatParams->m_uavCpuDescAllocations;
			}
			break;
			default:
				SEAssertF("Invalid range type");
			}

			SEAssert(descriptorAllocation->IsValid(), "Descriptor is not valid");


			// If a depth resource is used as both an input and target, we've already recorded the transitions
			if (!skipTransition)
			{
				TransitionResource(texture, toState, srcMip);
			}

			m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
				rootSigEntry->m_index,
				*descriptorAllocation,
				rootSigEntry->m_tableEntry.m_offset,
				1);
		}
	}


	void CommandList::TransitionResourceInternal(
		ID3D12Resource* resource,
		uint32_t totalSubresources, 
		D3D12_RESOURCE_STATES toState, 
		uint32_t targetSubresource, 
		uint32_t numFaces,
		uint32_t numMips)
	{
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		barriers.reserve(totalSubresources);

		auto InsertBarrier = [this, &resource, &barriers](uint32_t subresourceIdx, D3D12_RESOURCE_STATES toState)
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

		// Transition the appropriate subresources:
		if (targetSubresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
		{
			// We can only transition ALL subresources in a single barrier if the before state is the same for all
			// subresources. If we have any pending transitions for individual subresources, this is not the case: We
			// must transition each pending subresource individually to ensure all subresources have the correct before
			// and after state.

			// We need to transition 1-by-1 if there are individual pending subresource states, and we've got an ALL transition
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

						InsertBarrier(pendingSubresourceIdx, toState);
					}
				}
			}

			// We didn't need to process our transitions one-by-one: Submit a single ALL transition:
			if (doTransitionAllSubresources)
			{
				InsertBarrier(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, toState);
			}
		}
		else
		{
			// Transition the target mip level for each face
			for (uint32_t faceIdx = 0; faceIdx < numFaces; faceIdx++)
			{
				// TODO: We should be able to batch multiple transitions into a single call
				const uint32_t subresourceIdx = (faceIdx * numMips) + targetSubresource;

				InsertBarrier(subresourceIdx, toState);
			}
		}

		if (!barriers.empty()) // Might not have recored a barrier if it's the 1st time we've seen a resource
		{
			// Submit all of our transitions in a single batch
			ResourceBarrier(
				static_cast<uint32_t>(barriers.size()),
				barriers.data());
		}
	}


	void CommandList::TransitionResource(
		ID3D12Resource* resource, uint32_t totalSubresources, D3D12_RESOURCE_STATES toState, uint32_t targetSubresource)
	{
		TransitionResourceInternal(resource, totalSubresources, toState, targetSubresource, 1, 1);
	}


	void CommandList::TransitionResource(
		re::Texture const* texture, D3D12_RESOURCE_STATES toState, uint32_t mipLevel)
	{
		dx12::Texture::PlatformParams const* texPlatParams =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		ID3D12Resource* const resource = texPlatParams->m_textureResource.Get();
		
		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		TransitionResourceInternal(
			resource, texture->GetTotalNumSubresources(), toState, mipLevel, texParams.m_faces, texture->GetNumMips());
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


	void CommandList::InsertUAVBarrier(std::shared_ptr<re::Texture> texture)
	{
		InsertUAVBarrier(texture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>()->m_textureResource.Get());
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