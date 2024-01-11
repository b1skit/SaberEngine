// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Batch.h"
#include "CastUtils.h"
#include "Config.h"
#include "Context_DX12.h"
#include "CommandList_DX12.h"
#include "Debug_DX12.h"
#include "MeshPrimitive.h"
#include "ParameterBlock.h"
#include "ParameterBlock_DX12.h"
#include "RenderManager.h"
#include "RootSignature_DX12.h"
#include "SwapChain_DX12.h"
#include "Texture.h"
#include "Texture_DX12.h"
#include "TextureTarget.h"
#include "TextureTarget_DX12.h"
#include "VertexStream.h"
#include "VertexStream_DX12.h"

using Microsoft::WRL::ComPtr;

//#define DEBUG_RESOURCE_TRANSITIONS


namespace
{
	using dx12::CheckHResult;


	ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(
		ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type, std::wstring const& name)
	{
		SEAssert("Device cannot be null", device);

		ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;

		HRESULT hr = device->CreateCommandAllocator(
			type, // Copy, compute, direct draw, etc
			IID_PPV_ARGS(&commandAllocator)); // IID_PPV_ARGS: RIID & interface pointer
		CheckHResult(hr, "Failed to create command allocator");

		commandAllocator->SetName(name.c_str());

		hr = commandAllocator->Reset();
		CheckHResult(hr, "Failed to reset command allocator");

		return commandAllocator;
	}


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

		const std::string debugStr = std::format("{}: Texture \"{}\", mip {}\n{}{} -> {}",
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
		case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE: return CommandListType::VideoDecode;
		case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS: return CommandListType::VideoProcess;
		case D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE: return CommandListType::VideoEncode;
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
		SEAssert("Device cannot be null", device);

		// Name the command list with a monotonically-increasing index to make it easier to identify
		const std::wstring commandListname = std::wstring(
			GetCommandListTypeWName(type)) +
			L"_CommandList_#" + std::to_wstring(k_commandListNumber);

		m_commandAllocator = CreateCommandAllocator(device, m_d3dType, commandListname + L"_CommandAllocator");

		// Create the command list:
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		HRESULT hr = device->CreateCommandList(
			deviceNodeMask,
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
	}


	void CommandList::SetPipelineState(dx12::PipelineState const& pso)
	{
		if (m_currentPSO == &pso)
		{
			return;
		}
		m_currentPSO = &pso;

		ID3D12PipelineState* pipelineState = pso.GetD3DPipelineState();
		SEAssert("Pipeline state is null. This is unexpected", pipelineState);

		m_commandList->SetPipelineState(pipelineState);
	}


	void CommandList::SetGraphicsRootSignature(dx12::RootSignature const* rootSig)
	{
		SEAssert("Only graphics command lists can have a graphics/direct root signature",
			m_d3dType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT);

		if (m_currentRootSignature == rootSig)
		{
			return;
		}
		m_currentRootSignature = rootSig;

		m_gpuCbvSrvUavDescriptorHeaps->ParseRootSignatureDescriptorTables(rootSig);

		ID3D12RootSignature* rootSignature = rootSig->GetD3DRootSignature();
		SEAssert("Root signature is null. This is unexpected", rootSignature);

		m_commandList->SetGraphicsRootSignature(rootSignature);
	}


	void CommandList::SetComputeRootSignature(dx12::RootSignature const* rootSig)
	{
		SEAssert("Only graphics or compute command lists can have a compute root signature", 
			m_d3dType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT || 
			m_d3dType == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE);

		if (m_currentRootSignature == rootSig)
		{
			return;
		}
		m_currentRootSignature = rootSig;

		m_gpuCbvSrvUavDescriptorHeaps->ParseRootSignatureDescriptorTables(rootSig);

		ID3D12RootSignature* rootSignature = rootSig->GetD3DRootSignature();
		SEAssert("Root signature is null. This is unexpected", rootSignature);

		m_commandList->SetComputeRootSignature(rootSignature);
	}


	void CommandList::SetParameterBlock(re::ParameterBlock const* parameterBlock)
	{
		SEAssert("Root signature has not been set", m_currentRootSignature != nullptr);

		dx12::ParameterBlock::PlatformParams const* pbPlatParams =
			parameterBlock->GetPlatformParams()->As<dx12::ParameterBlock::PlatformParams*>();

		RootSignature::RootParameter const* rootSigEntry = 
			m_currentRootSignature->GetRootSignatureEntry(parameterBlock->GetName());
		SEAssert("Invalid root signature entry", 
			rootSigEntry ||
			en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false);

		if (rootSigEntry)
		{
			const uint32_t rootSigIdx = rootSigEntry->m_index;

			switch (parameterBlock->GetPlatformParams()->m_dataType)
			{
			case re::ParameterBlock::PBDataType::SingleElement:
			{
				m_gpuCbvSrvUavDescriptorHeaps->SetInlineCBV(
					rootSigIdx,
					pbPlatParams->m_resource.Get(),
					pbPlatParams->m_heapByteOffset);
			}
			break;
			case re::ParameterBlock::PBDataType::Array:
			{
				m_gpuCbvSrvUavDescriptorHeaps->SetInlineSRV(
					rootSigIdx,
					pbPlatParams->m_resource.Get(),
					pbPlatParams->m_heapByteOffset);
			}
			break;
			default:
				SEAssertF("Invalid PBDataType");
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
			SEAssert("Index stream cannot be null for indexed draws", indexStream);

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
			SEAssert("Position stream cannot be null", positionStream);

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
		SEAssert("Invalid vertex streams received", streams && count > 0);

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
		SEAssert("Target texture cannot be null", depthTarget);

		if (depthTarget->GetClearMode() == re::TextureTarget::TargetParams::ClearMode::Enabled)
		{
			std::shared_ptr<re::Texture> depthTex = depthTarget->GetTexture();

			re::Texture::TextureParams const& depthTexParams = depthTex->GetTextureParams();

			SEAssert("Target texture must be a depth target",
				(depthTexParams.m_usage & re::Texture::Usage::DepthTarget) != 0);

			const uint32_t numDepthMips = depthTex->GetNumMips();
			SEAssert("Depth target has mips. This is unexpected", numDepthMips == 1);

			re::TextureTarget::TargetParams const& depthTargetParams = depthTarget->GetTargetParams();

			dx12::TextureTarget::PlatformParams* depthTargetPlatParams =
				depthTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

			if (depthTargetParams.m_targetFace == re::TextureTarget::k_allFaces)
			{
				SEAssert("We're (currently) expecting the a cubemap",
					depthTexParams.m_dimension == re::Texture::Dimension::TextureCubeMap);

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
		SEAssert("Target texture cannot be null", colorTarget);

		SEAssert("Target texture must be a color target", 
			(colorTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::ColorTarget) ||
			(colorTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::SwapchainColorProxy));

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
		SEAssert("This method is not valid for compute or copy command lists",
			m_type != CommandListType::Compute && m_type != CommandListType::Copy);

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
				SEAssert("We're (currently) expecting a cubemap",
					depthStencilTarget->GetTexture()->GetTextureParams().m_dimension == re::Texture::Dimension::TextureCubeMap);

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
		SEAssert("It is not possible to attach a depth buffer as a target to a compute shader", 
			textureTargetSet.GetDepthStencilTarget() == nullptr);

		SEAssert("This function should only be called from compute command lists", m_type == CommandListType::Compute);
		SEAssert("Pipeline is not currently set", m_currentPSO);

		// Track the D3D resources we've seen during this call, to help us decide whether to insert a UAV barrier or 
		// not. We search in reverse order because it seems more natural that the same resource would be attached in
		// sequence, but this is just an assumption. The search is likely to be very short either way
		std::vector<ID3D12Resource*> seenResources;
		seenResources.reserve(textureTargetSet.GetNumColorTargets());
		auto ResourceWasTransitionedInThisCall = [&seenResources](ID3D12Resource const* newResource) -> bool
		{
			std::vector<ID3D12Resource*>::iterator itr = seenResources.end();
			while (itr != seenResources.begin())
			{
				itr--; // .end() is invalid; Need to decrement first
				if (newResource == *itr)
				{
					return true;
				}
			}
			return false;
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

			SEAssert("It is unexpected that we're trying to attach a texture with DepthTarget usage to a compute shader",
				(colorTex->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget) == 0);

			// We bind our UAV targets by mapping TextureTargetSet index to register/bind point values
			RootSignature::RootParameter const* rootSigEntry = m_currentRootSignature->GetRootSignatureEntry(
					RootSignature::DescriptorType::UAV,
					static_cast<uint8_t>(i));

			SEAssert("Invalid root signature entry",
				rootSigEntry || en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false);

			if (rootSigEntry)
			{
				SEAssert("We currently assume all textures belong to descriptor tables",
					rootSigEntry->m_type == RootSignature::RootParameter::Type::DescriptorTable);

				SEAssert("Compute shaders can only write to UAVs",
					rootSigEntry->m_tableEntry.m_type == dx12::RootSignature::DescriptorType::UAV);

				re::TextureTarget::TargetParams const& targetParams = colorTarget.GetTargetParams();

				dx12::Texture::PlatformParams* texPlatParams =
					colorTex->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

				const uint32_t targetMip = targetParams.m_targetMip;

				SEAssert("Not enough UAV descriptors",
					targetMip < texPlatParams->m_uavCpuDescAllocations.size());

				dx12::DescriptorAllocation const* descriptorAllocation =
					&texPlatParams->m_uavCpuDescAllocations[targetMip];

				m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
					rootSigEntry->m_index,
					*descriptorAllocation,
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
				seenResources.emplace_back(resource);

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
		SEAssert("This method is not valid for compute or copy command lists",
			m_type != CommandListType::Compute && m_type != CommandListType::Copy);

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
		SEAssert("Expected a copy command list", m_type == dx12::CommandListType::Copy);

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
			void const* initialData = texture->GetTexelData(faceIdx);
			SEAssert("Initial data cannot be null", initialData);

			subresourceData.emplace_back(D3D12_SUBRESOURCE_DATA{
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
		SEAssert("UpdateSubresources returned 0 bytes. This is unexpected", bufferSizeResult > 0);

		// Transition to the copy destination state:
		TransitionResource(texture, D3D12_RESOURCE_STATE_COPY_DEST, re::Texture::k_allMips);
	}


	void CommandList::UpdateSubresources(
		re::VertexStream const* stream, ID3D12Resource* intermediate, size_t intermediateOffset)
	{
		SEAssert("Expected a copy command list", m_type == dx12::CommandListType::Copy);

		dx12::VertexStream::PlatformParams const* streamPlatformParams =
			stream->GetPlatformParams()->As<dx12::VertexStream::PlatformParams const*>();

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
		SEAssert("UpdateSubresources returned 0 bytes. This is unexpected", bufferSizeResult > 0);

		TransitionResource(streamPlatformParams->m_bufferResource.Get(), 1, D3D12_RESOURCE_STATE_COPY_DEST, 0);
	}


	void CommandList::SetTexture(
		std::string const& shaderName, re::Texture const* texture, uint32_t srcMip, bool skipTransition)
	{
		SEAssert("Pipeline is not currently set", m_currentPSO);

		SEAssert("Unexpected mip level",
			srcMip < texture->GetNumMips() ||
			srcMip == re::Texture::k_allMips);

		dx12::Texture::PlatformParams const* texPlatParams = 
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams const*>();

		RootSignature::RootParameter const* rootSigEntry =
			m_currentRootSignature->GetRootSignatureEntry(shaderName);
		SEAssert("Invalid root signature entry",
			rootSigEntry ||
			en::Config::Get()->KeyExists(en::ConfigKeys::k_strictShaderBindingCmdLineArg) == false);

		if (rootSigEntry)
		{
			SEAssert("We currently assume all textures belong to descriptor tables",
				rootSigEntry->m_type == RootSignature::RootParameter::Type::DescriptorTable);

			D3D12_RESOURCE_STATES toState = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
			dx12::DescriptorAllocation const* descriptorAllocation = nullptr;
			switch (rootSigEntry->m_tableEntry.m_type)
			{
			case dx12::RootSignature::DescriptorType::SRV:
			{
				SEAssert("Unexpected command list type",
					m_d3dType == D3D12_COMMAND_LIST_TYPE_COMPUTE || m_d3dType == D3D12_COMMAND_LIST_TYPE_DIRECT);

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

			SEAssert("Descriptor is not valid", descriptorAllocation->IsValid());


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

		auto InsertBarrier = [this, &resource, &barriers, &toState](uint32_t subresourceIdx)
			{
				// If we've already seen this resource before, we can record the transition now (as we prepend any initial
				// transitions when submitting the command list)	
				if (m_resourceStates.HasResourceState(resource, subresourceIdx)) // Is the subresource idx (or ALL) in our known states list?
				{
					const D3D12_RESOURCE_STATES currentKnownState = m_resourceStates.GetResourceState(resource, subresourceIdx);

#if defined(DEBUG_RESOURCE_TRANSITIONS)
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
#if defined(DEBUG_RESOURCE_TRANSITIONS)
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

						const D3D12_RESOURCE_STATES fromState = pendingState.second;
						if (fromState == toState)
						{
							continue;
						}

						const uint32_t pendingSubresourceIdx = pendingState.first;

						barriers.emplace_back(D3D12_RESOURCE_BARRIER{
							.Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
							.Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE,
							.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
								.pResource = resource,
								.Subresource = pendingSubresourceIdx,
								.StateBefore = fromState,
								.StateAfter = toState}
							});

						m_resourceStates.SetResourceState(resource, toState, pendingSubresourceIdx);
						
#if defined(DEBUG_RESOURCE_TRANSITIONS)
						DebugResourceTransitions(
							*this, dx12::GetDebugName(resource).c_str(), fromState, toState, pendingSubresourceIdx);
#endif
					}
				}
			}

			// We didn't need to process our transitions one-by-one: Submit a single ALL transition:
			if (doTransitionAllSubresources)
			{
				InsertBarrier(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
			}
		}
		else
		{
			// Transition the target mip level for each face
			for (uint32_t faceIdx = 0; faceIdx < numFaces; faceIdx++)
			{
				// TODO: We should be able to batch multiple transitions into a single call
				const uint32_t subresourceIdx = (faceIdx * numMips) + targetSubresource;

				InsertBarrier(subresourceIdx);
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


	void CommandList::InsertUAVBarrier(
		std::shared_ptr<re::Texture> texture)
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

		ID3D12Resource* resource =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>()->m_textureResource.Get();

		const D3D12_RESOURCE_BARRIER barrier{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
				.Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.UAV = D3D12_RESOURCE_UAV_BARRIER{
					.pResource = resource}
		};

		// TODO: Support batching of multiple barriers
		ResourceBarrier(1, &barrier);
	}


	void CommandList::ResourceBarrier(uint32_t numBarriers, D3D12_RESOURCE_BARRIER const* barriers)
	{
		SEAssert("Attempting to submit 0 barriers", numBarriers > 0);

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