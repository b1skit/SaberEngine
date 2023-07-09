// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Config.h"
#include "Context_DX12.h"
#include "CommandList_DX12.h"
#include "Debug_DX12.h"
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


namespace
{
	using dx12::CheckHResult;


	ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type)
	{
		ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;

		HRESULT hr = device->CreateCommandAllocator(
			type, // Copy, compute, direct draw, etc
			IID_PPV_ARGS(&commandAllocator)); // IID_PPV_ARGS: RIID & interface pointer
		CheckHResult(hr, "Failed to create command allocator");

		hr = commandAllocator->Reset();
		CheckHResult(hr, "Failed to reset command allocator");

		return commandAllocator;
	}
}


namespace dx12
{
	size_t CommandList::s_commandListNumber = 0;


	CommandList::CommandList(ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type)
		: m_commandList(nullptr)
		, m_type(type)
		, m_commandAllocator(nullptr)
		, m_fenceValue(0)
		, k_commandListNumber(s_commandListNumber++)
		, m_gpuCbvSrvUavDescriptorHeaps(nullptr)
		, m_currentGraphicsRootSignature(nullptr)
		, m_currentPSO(nullptr)
		
	{
		m_commandAllocator = CreateCommandAllocator(device, type);

		// Create the command list:
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		HRESULT hr = device->CreateCommandList(
			deviceNodeMask,
			m_type,							// Direct draw/compute/copy/etc
			m_commandAllocator.Get(),		// The command allocator the command lists will be created on
			nullptr,						// Optional: Command list initial pipeline state
			IID_PPV_ARGS(&m_commandList));	// IID_PPV_ARGS: RIID & destination for the populated command list
		CheckHResult(hr, "Failed to create command list");

		// Name the command list with a monotonically-increasing index
		const std::wstring commandListname = L"CommandList_#" + std::to_wstring(k_commandListNumber);
		m_commandList->SetName(commandListname.c_str());

		// Set the descriptor heaps (unless we're a copy command list):
		if (m_type != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY)
		{
			// Create our GPU-visible descriptor heaps:
			m_gpuCbvSrvUavDescriptorHeaps = std::make_unique<GPUDescriptorHeap>(
				m_commandList.Get(),
				m_type,
				D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
		m_type = D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_NONE;
		m_commandAllocator = nullptr;
		m_fenceValue = 0;
		m_gpuCbvSrvUavDescriptorHeaps = nullptr;
		m_currentGraphicsRootSignature = nullptr;
		m_currentPSO = nullptr;
	}


	void CommandList::Reset()
	{
		m_currentGraphicsRootSignature = nullptr;
		m_currentPSO = nullptr;

		// Reset the command allocator BEFORE we reset the command list (to avoid leaking memory)
		m_commandAllocator->Reset();

		m_resourceStates.Reset();

		// Note: pso is optional here; nullptr sets a dummy PSO
		HRESULT hr = m_commandList->Reset(m_commandAllocator.Get(), nullptr);
		CheckHResult(hr, "Failed to reset command list");

		// Re-bind the descriptor heaps (unless we're a copy command list):
		if (m_type != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY)
		{
			// Reset the GPU descriptor heap managers:
			m_gpuCbvSrvUavDescriptorHeaps->Reset();

			constexpr uint8_t k_numHeaps = 1;
			ID3D12DescriptorHeap* descriptorHeaps[k_numHeaps] = {
				m_gpuCbvSrvUavDescriptorHeaps->GetD3DDescriptorHeap()
			};
			m_commandList->SetDescriptorHeaps(k_numHeaps, descriptorHeaps);
		}
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
		if (m_currentGraphicsRootSignature == rootSig)
		{
			return;
		}

		m_currentGraphicsRootSignature = rootSig;

		m_gpuCbvSrvUavDescriptorHeaps->ParseRootSignatureDescriptorTables(rootSig);

		ID3D12RootSignature* rootSignature = rootSig->GetD3DRootSignature();
		SEAssert("Root signature is null. This is unexpected", rootSignature);

		m_commandList->SetGraphicsRootSignature(rootSignature);
	}


	void CommandList::SetParameterBlock(re::ParameterBlock const* parameterBlock)
	{
		// TODO: Handle setting parameter blocks for different root signature types (e.g. compute)
		// -> Can probably just assume only 1 single root signature type will be set, and the other will be null?
		SEAssert("Root signature has not been set", m_currentGraphicsRootSignature != nullptr);

		dx12::ParameterBlock::PlatformParams const* pbPlatParams =
			parameterBlock->GetPlatformParams()->As<dx12::ParameterBlock::PlatformParams*>();

		RootSignature::RootParameter const* rootSigEntry = 
			m_currentGraphicsRootSignature->GetRootSignatureEntry(parameterBlock->GetName());
		SEAssert("Invalid root signature entry", 
			rootSigEntry ||
			en::Config::Get()->ValueExists(en::Config::k_relaxedShaderBindingCmdLineArg) == true);

		if (rootSigEntry)
		{
			const uint32_t rootSigIdx = rootSigEntry->m_index;

			switch (parameterBlock->GetPlatformParams()->m_dataType)
			{
			case re::ParameterBlock::PBDataType::SingleElement:
			{
				m_gpuCbvSrvUavDescriptorHeaps->SetInlineCBV(
					rootSigIdx,							// Root signature index 
					pbPlatParams->m_resource.Get());	// Resource
			}
			break;
			case re::ParameterBlock::PBDataType::Array:
			{
				m_gpuCbvSrvUavDescriptorHeaps->SetInlineSRV(
					rootSigIdx,							// Root signature index 
					pbPlatParams->m_resource.Get());	// Resource
			}
			break;
			default:
				SEAssertF("Invalid PBDataType");
			}
		}
	}


	void CommandList::SetVertexBuffer(uint32_t slot, re::VertexStream const* stream) const
	{
		m_commandList->IASetVertexBuffers(
			slot, 
			1, 
			&stream->GetPlatformParams()->As<dx12::VertexStream::PlatformParams_Vertex*>()->m_vertexBufferView);
	}


	void CommandList::SetVertexBuffers(std::vector<std::shared_ptr<re::VertexStream>> const& streams) const
	{
		SEAssert("The position stream is mandatory", 
			streams.size() > 0 && streams[re::MeshPrimitive::Slot::Position] != nullptr);

		uint32_t currentStartSlot = 0;

		std::vector<D3D12_VERTEX_BUFFER_VIEW> streamViews;
		streamViews.reserve(streams.size());

		for (uint32_t streamIdx = 0; streamIdx < static_cast<uint32_t>(streams.size()); streamIdx++)
		{
			if (streamIdx == static_cast<size_t>(re::MeshPrimitive::Slot::Indexes))
			{
				break;
			}
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
		ClearDepthTarget(depthTarget, depthTarget->GetTexture()->GetTextureParams().m_clearColor.r);
	}


	void CommandList::ClearDepthTarget(re::TextureTarget const* depthTarget, float clearColor) const
	{
		SEAssert("Target texture must be a depth target",
			depthTarget &&
			depthTarget->GetTexture()->GetTextureParams().m_usage == re::Texture::Usage::DepthTarget);

		dx12::TextureTarget::PlatformParams* depthPlatParams =
			depthTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

		D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptor = depthPlatParams->m_rtvDsvDescriptor.GetBaseDescriptor();

		m_commandList->ClearDepthStencilView(
			dsvDescriptor,
			D3D12_CLEAR_FLAG_DEPTH,
			clearColor,
			0,
			0,
			nullptr);
	}


	void CommandList::ClearColorTarget(re::TextureTarget const* colorTarget) const
	{
		SEAssert("Target texture cannot be null", colorTarget);
		ClearColorTarget(colorTarget, colorTarget->GetTexture()->GetTextureParams().m_clearColor);
	}


	void CommandList::ClearColorTarget(re::TextureTarget const* colorTarget, glm::vec4 clearColor) const
	{
		SEAssert("Target texture must be a color target", 
			colorTarget && 
			colorTarget->GetTexture()->GetTextureParams().m_usage == re::Texture::Usage::ColorTarget ||
			colorTarget->GetTexture()->GetTextureParams().m_usage == re::Texture::Usage::SwapchainColorProxy);

		dx12::TextureTarget::PlatformParams* targetParams =
			colorTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

		m_commandList->ClearRenderTargetView(
			targetParams->m_rtvDsvDescriptor.GetBaseDescriptor(),
			&clearColor.r,
			0,			// Number of rectangles in the proceeding D3D12_RECT ptr
			nullptr);	// Ptr to an array of rectangles to clear in the resource view. Clears entire view if null
	}


	void CommandList::ClearColorTargets(re::TextureTargetSet const& targetSet) const
	{
		for (std::unique_ptr<re::TextureTarget> const& target : targetSet.GetColorTargets())
		{
			ClearColorTarget(target.get());
		}
	}


	void CommandList::SetRenderTargets(re::TextureTargetSet& targetSet) const
	{
		SEAssertF("NOTE: This is untested. It's probably fine, but asserting to save some future head scratching...");

		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> colorTargetDescriptors;
		colorTargetDescriptors.reserve(targetSet.GetColorTargets().size());

		uint32_t numColorTargets = 0;
		for (uint8_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			if (targetSet.GetColorTarget(i))
			{
				dx12::TextureTarget::PlatformParams* targetPlatParams =
					targetSet.GetColorTarget(i)->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

				colorTargetDescriptors.emplace_back(targetPlatParams->m_rtvDsvDescriptor.GetBaseDescriptor());
				numColorTargets++;
			}
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptor{};
		re::TextureTarget const* depthStencilTarget = targetSet.GetDepthStencilTarget();
		if (depthStencilTarget)
		{
			dx12::TextureTarget::PlatformParams* depthPlatParams =
				depthStencilTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();
			dsvDescriptor = depthPlatParams->m_rtvDsvDescriptor.GetBaseDescriptor();
		}
		
		// NOTE: isSingleHandleToDescRange == true specifies that the rtvs are contiguous in memory, thus N rtv 
		// descriptors will be found by offsetting from rtvs[0]. Otherwise, it is assumed rtvs is an array of descriptor
		// pointers
		m_commandList->OMSetRenderTargets(numColorTargets, &colorTargetDescriptors[0], false, &dsvDescriptor);
	}


	void CommandList::SetBackbufferRenderTarget() const
	{
		// TODO: The backbuffer should maintain multiple target sets (sharing a depth target texture). When we want to
		// set the backbuffer rendertarget, we should just call the standard SetRenderTargets function, and pass in the
		// appropriate target set. We should not have this separate SetBackbufferRenderTarget function

		re::Context const& context = re::RenderManager::Get()->GetContext();

		dx12::SwapChain::PlatformParams* swapChainParams =
			context.GetSwapChain().GetPlatformParams()->As<dx12::SwapChain::PlatformParams*>();

		const uint8_t backbufferIdx = dx12::SwapChain::GetBackBufferIdx(context.GetSwapChain());

		dx12::TextureTarget::PlatformParams* swapChainColorTargetPlatParams =
			swapChainParams->m_backbufferTargetSet->GetColorTarget(backbufferIdx)->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();

		const D3D12_CPU_DESCRIPTOR_HANDLE colorDescHandle = 
			swapChainColorTargetPlatParams->m_rtvDsvDescriptor.GetBaseDescriptor();

		dx12::TextureTarget::PlatformParams* depthTargetPlatParams =
			swapChainParams->m_backbufferTargetSet->GetDepthStencilTarget()->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();
		const D3D12_CPU_DESCRIPTOR_HANDLE depthDescHandle = 
			depthTargetPlatParams->m_rtvDsvDescriptor.GetBaseDescriptor();

		m_commandList->OMSetRenderTargets(
			1, 
			&colorDescHandle,
			false, 
			&depthDescHandle);
	}


	void CommandList::SetViewport(re::TextureTargetSet const& targetSet) const
	{
		dx12::TextureTargetSet::PlatformParams* targetSetParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		re::Viewport const& viewport = targetSet.Viewport();

		// TODO: We should only update this if it has changed!
		// TODO: OpenGL expects ints, DX12 expects floats. We should support both via the Viewport interface (eg. Union)
		targetSetParams->m_viewport = CD3DX12_VIEWPORT(
			static_cast<float>(viewport.xMin()),
			static_cast<float>(viewport.yMin()),
			static_cast<float>(viewport.Width()),
			static_cast<float>(viewport.Height()));

		m_commandList->RSSetViewports(1, &targetSetParams->m_viewport);

		// TODO: It is possible to have more than 1 viewport (eg. Geometry shaders), we should handle this (i.e. a 
		// viewport per target?)
	}


	void CommandList::SetScissorRect(re::TextureTargetSet const& targetSet) const
	{
		dx12::TextureTargetSet::PlatformParams* targetSetParams =
			targetSet.GetPlatformParams()->As<dx12::TextureTargetSet::PlatformParams*>();

		re::ScissorRect const& scissorRect = targetSet.ScissorRect();

		// TODO: We should only update this if it has changed!
		targetSetParams->m_scissorRect = CD3DX12_RECT(
			scissorRect.Left(),
			scissorRect.Top(),
			scissorRect.Right(),
			scissorRect.Bottom());

		m_commandList->RSSetScissorRects(1, &targetSetParams->m_scissorRect);
	}


	void CommandList::SetTexture(std::string const& shaderName, std::shared_ptr<re::Texture> texture)
	{
		dx12::Texture::PlatformParams* texPlatParams = 
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		// TODO: Handle this more intelligently
		TransitionResource(texPlatParams->m_textureResource.Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

		SEAssert("Pipeline is not currently set", m_currentPSO);
		
		RootSignature::RootParameter const* rootSigEntry =
			m_currentPSO->GetRootSignature()->GetRootSignatureEntry(shaderName);
		SEAssert("Invalid root signature entry",
			rootSigEntry ||
			en::Config::Get()->ValueExists(en::Config::k_relaxedShaderBindingCmdLineArg) == true);

		if (rootSigEntry)
		{
			SEAssert("We currently assume all textures belong to descriptor tables",
				rootSigEntry->m_type == RootSignature::RootParameter::Type::DescriptorTable);

			m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
				rootSigEntry->m_index,
				texPlatParams->m_cpuDescAllocation,
				rootSigEntry->m_tableEntry.m_offset,
				1);
		}
	}


	void CommandList::TransitionResource(
		ID3D12Resource* resource, D3D12_RESOURCE_STATES toState, uint32_t subresourceIdx)
	{
		if (m_resourceStates.HasResourceState(resource, subresourceIdx))
		{
			const D3D12_RESOURCE_STATES currentState = m_resourceStates.GetResourceState(resource, subresourceIdx);
			if (currentState == toState)
			{
				return; // Before and after states must be different
			}

			// If we've already seen this resource before, we can record the transition now (as we'll prepend any
			// initial transitions when submitting the command list)			
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				resource,
				currentState,
				toState,
				subresourceIdx,
				D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE);

			// TODO: Support batching of multiple barriers
			m_commandList->ResourceBarrier(1, &barrier);
		}

		// Record the new state after the transition:
		m_resourceStates.SetResourceState(resource, toState, subresourceIdx);
	}


	LocalResourceStateTracker const& CommandList::GetLocalResourceStates() const
	{
		return m_resourceStates;
	}
}