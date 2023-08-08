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
		SEAssert("Device cannot be null", device);

		ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;

		HRESULT hr = device->CreateCommandAllocator(
			type, // Copy, compute, direct draw, etc
			IID_PPV_ARGS(&commandAllocator)); // IID_PPV_ARGS: RIID & interface pointer
		CheckHResult(hr, "Failed to create command allocator");

		hr = commandAllocator->Reset();
		CheckHResult(hr, "Failed to reset command allocator");

		return commandAllocator;
	}


	constexpr bool IsWriteableState(D3D12_RESOURCE_STATES state)
	{
		switch (state)
		{
		case D3D12_RESOURCE_STATE_RENDER_TARGET:
		case D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
		case D3D12_RESOURCE_STATE_DEPTH_WRITE:
		case D3D12_RESOURCE_STATE_STREAM_OUT:
		case D3D12_RESOURCE_STATE_COPY_DEST:
		case D3D12_RESOURCE_STATE_RESOLVE_DEST:
		case D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE:
		case D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE:
		case D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE:
			return true;
		default:
			return false;
		}
	}
}


namespace dx12
{
	size_t CommandList::s_commandListNumber = 0;


	constexpr wchar_t const* const CommandList::GetCommandListTypeName(dx12::CommandListType type)
	{
		switch (type)
		{
		case CommandListType::Direct: return L"Direct";
		case CommandListType::Bundle: return L"Bundle";
		case CommandListType::Compute: L"Compute";
		case CommandListType::Copy: return L"Copy";
		case CommandListType::VideoDecode: return L"VideoDecode";
		case CommandListType::VideoProcess: return L"VideoProcess";
		case CommandListType::VideoEncode: return L"VideoEncode";
		default:
			SEAssertF("Invalid command list type");
		}
		return L"InvalidType";
	};


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


	CommandList::CommandList(ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type)
		: m_commandList(nullptr)
		, m_type(type)
		, m_commandAllocator(nullptr)
		, m_reuseFenceValue(0)
		, k_commandListNumber(s_commandListNumber++)
		, m_gpuCbvSrvUavDescriptorHeaps(nullptr)
		, m_currentRootSignature(nullptr)
		, m_currentPSO(nullptr)
	{
		SEAssert("Device cannot be null", device);

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

		// Name the command list with a monotonically-increasing index to make it easier to identify
		const std::wstring commandListname = std::wstring(
			GetCommandListTypeName(TranslateToSECommandListType(type))) + 
			L"_CommandList_#" + std::to_wstring(k_commandListNumber);
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
		m_reuseFenceValue = 0;
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

		m_reuseFenceValue = 0;
		m_accessedResources.clear();
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
			m_type == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT);

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
			m_type == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_DIRECT || 
			m_type == D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE);

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
		// TODO: Handle setting parameter blocks for different root signature types (e.g. compute)
		// -> Can probably just assume only 1 single root signature type will be set, and the other will be null?
		SEAssert("Root signature has not been set", m_currentRootSignature != nullptr);

		dx12::ParameterBlock::PlatformParams const* pbPlatParams =
			parameterBlock->GetPlatformParams()->As<dx12::ParameterBlock::PlatformParams*>();

		RootSignature::RootParameter const* rootSigEntry = 
			m_currentRootSignature->GetRootSignatureEntry(parameterBlock->GetName());
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
		ClearDepthTarget(depthTarget, depthTarget->GetTargetParams().m_clearColor.r);
	}


	void CommandList::ClearDepthTarget(re::TextureTarget const* depthTarget, float clearColor) const
	{
		SEAssert("Target texture must be a depth target",
			depthTarget &&
			(depthTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget));

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
		ClearColorTarget(colorTarget, colorTarget->GetTargetParams().m_clearColor);
	}


	void CommandList::ClearColorTarget(re::TextureTarget const* colorTarget, glm::vec4 clearColor) const
	{
		SEAssert("Target texture must be a color target", 
			colorTarget && 
			(colorTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::ColorTarget) ||
			(colorTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::SwapchainColorProxy));

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
		for (re::TextureTarget const& target : targetSet.GetColorTargets())
		{
			if (target.HasTexture())
			{
				ClearColorTarget(&target);
			}
		}
	}


	void CommandList::SetRenderTargets(re::TextureTargetSet const& targetSet)
	{
		SEAssert("This method is not valid for compute or copy command lists",
			m_type != CommandListType::Compute && m_type != CommandListType::Copy);

		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> colorTargetDescriptors;
		colorTargetDescriptors.reserve(targetSet.GetColorTargets().size());

		uint32_t numColorTargets = 0;
		for (uint8_t i = 0; i < targetSet.GetColorTargets().size(); i++)
		{
			re::TextureTarget const* target = targetSet.GetColorTarget(i);
			if (target)
			{
				dx12::Texture::PlatformParams* texPlatParams =
					target->GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

				// Insert our resource transition:
				TransitionResource(
					target->GetTexture(),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					target->GetTargetParams().m_targetSubesource);

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
			dx12::Texture::PlatformParams* depthTexPlatParams =
				depthStencilTarget->GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

			// Insert our resource transition:
			TransitionResource(
				depthStencilTarget->GetTexture(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				depthStencilTarget->GetTargetParams().m_targetSubesource);

			dx12::TextureTarget::PlatformParams* depthTargetPlatParams =
				depthStencilTarget->GetPlatformParams()->As<dx12::TextureTarget::PlatformParams*>();
			dsvDescriptor = depthTargetPlatParams->m_rtvDsvDescriptor.GetBaseDescriptor();
		}

		// NOTE: isSingleHandleToDescRange == true specifies that the rtvs are contiguous in memory, thus N rtv 
		// descriptors will be found by offsetting from rtvs[0]. Otherwise, it is assumed rtvs is an array of descriptor
		// pointers
		m_commandList->OMSetRenderTargets(
			numColorTargets,
			colorTargetDescriptors.data(),
			false,			// TODO: Are our render target descriptors ever a contiguous range? Can they be made to be?
			dsvDescriptor.ptr == 0 ? nullptr : &dsvDescriptor);

		// Set the viewport and scissor rectangles:
		SetViewport(targetSet);
		SetScissorRect(targetSet);
	}


	void CommandList::SetComputeTargets(re::TextureTargetSet const& textureTargetSet)
	{
		SEAssert("It is not possible to attach a depth buffer as a target to a compute shader", 
			textureTargetSet.GetDepthStencilTarget() == nullptr);

		SEAssert("This function should only be called from compute command lists", m_type == CommandListType::Compute);
		SEAssert("Pipeline is not currently set", m_currentPSO);

		std::vector<re::TextureTarget> const& texTargets = textureTargetSet.GetColorTargets();
		for (size_t i = 0; i < texTargets.size(); i++)
		{
			if (!texTargets[i].HasTexture())
			{
				continue;
			}
			re::TextureTarget const* texTarget = &texTargets[i];

			SEAssert("It is unexpected that we're trying to attach a texture with DepthTarget usage to a compute shader",
				(texTarget->GetTexture()->GetTextureParams().m_usage & re::Texture::Usage::DepthTarget) == 0);

			// We bind our UAV targets by mapping TextureTargetSet index to register/bind point values
			RootSignature::RootParameter const* rootSigEntry = m_currentRootSignature->GetRootSignatureEntry(
					RootSignature::DescriptorType::UAV,
					static_cast<uint8_t>(i));

			SEAssert("Invalid root signature entry",
				rootSigEntry || en::Config::Get()->ValueExists(en::Config::k_relaxedShaderBindingCmdLineArg) == true);

			if (rootSigEntry)
			{
				SEAssert("We currently assume all textures belong to descriptor tables",
					rootSigEntry->m_type == RootSignature::RootParameter::Type::DescriptorTable);

				SEAssert("Compute shaders can only write to UAVs",
					rootSigEntry->m_tableEntry.m_type == dx12::RootSignature::DescriptorType::UAV);


				re::TextureTarget::TargetParams const& targetParams = texTarget->GetTargetParams();
				
				dx12::Texture::PlatformParams* texPlatParams =
					texTarget->GetTexture()->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

				SEAssert("Not enought view desciptors",
					targetParams.m_targetSubesource < texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::UAV].size());

				dx12::DescriptorAllocation const* descriptorAllocation =
					&texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::UAV][targetParams.m_targetSubesource];

				m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
					rootSigEntry->m_index,
					*descriptorAllocation,
					rootSigEntry->m_tableEntry.m_offset,
					1);

				// Insert our resource transition:
				TransitionResource(
					texTarget->GetTexture(),
					D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
					texTarget->GetTargetParams().m_targetSubesource);

				// TODO: Should we also insert a D3D12_RESOURCE_UAV_BARRIER?
				// Not needed between 2 draw/dispatch calls that only read a UAV
				// Not needed between 2 draw/dispatch calls that write to a UAV IFF the writes can be executed in any order
				// -> Only needed to ensure write ordering
			}
		}
	}


	void CommandList::SetViewport(re::TextureTargetSet const& targetSet) const
	{
		SEAssert("This method is not valid for compute or copy command lists",
			m_type != CommandListType::Compute && m_type != CommandListType::Copy);

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

		const uint32_t numViewports = 1;
		m_commandList->RSSetViewports(numViewports, &targetSetParams->m_viewport);

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

		const uint32_t numScissorRects = 1; // 1 per viewport, in an array of viewports
		m_commandList->RSSetScissorRects(numScissorRects, &targetSetParams->m_scissorRect);
	}


	void CommandList::SetTexture(std::string const& shaderName, std::shared_ptr<re::Texture> texture, uint32_t subresource)
	{
		SEAssert("Pipeline is not currently set", m_currentPSO);

		dx12::Texture::PlatformParams* texPlatParams = 
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		RootSignature::RootParameter const* rootSigEntry =
			m_currentRootSignature->GetRootSignatureEntry(shaderName);
		SEAssert("Invalid root signature entry",
			rootSigEntry ||
			en::Config::Get()->ValueExists(en::Config::k_relaxedShaderBindingCmdLineArg) == true);

		if (rootSigEntry)
		{
			SEAssert("We currently assume all textures belong to descriptor tables",
				rootSigEntry->m_type == RootSignature::RootParameter::Type::DescriptorTable);

			if (subresource > texture->GetNumMips())
			{
				subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			}

			dx12::DescriptorAllocation const* descriptorAllocation = nullptr;
			switch (rootSigEntry->m_tableEntry.m_type)
			{
			case dx12::RootSignature::DescriptorType::SRV:
			{
				SEAssert("Unexpected command list type",
					m_type == D3D12_COMMAND_LIST_TYPE_COMPUTE || m_type == D3D12_COMMAND_LIST_TYPE_DIRECT);

				D3D12_RESOURCE_STATES toState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				if (m_type != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COMPUTE)
				{
					toState |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				}
				
				TransitionResource(texture,
					toState,
					subresource);

				SEAssert("TODO: It's currently unexpected a texture resources has > 1 SRV",
					texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::SRV].size() == 1);

				descriptorAllocation = &texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::SRV][0];
			}
			break;
			case dx12::RootSignature::DescriptorType::UAV:
			{
				// This is for UAV *inputs*
				SEAssertF("TODO: Implement this. Need to figure out how to specify the appropriate mip level/subresource index");

				// Note: We don't (shouldn't?) need to record a modification fence value to the texture resource here, 
				// since it's being used as an input

				//descriptorAllocation = &texPlatParams->m_viewCpuDescAllocations[dx12::Texture::View::UAV];
			}
			break;
			default:
				SEAssertF("Invalid range type");
			}

			m_gpuCbvSrvUavDescriptorHeaps->SetDescriptorTable(
				rootSigEntry->m_index,
				*descriptorAllocation,
				rootSigEntry->m_tableEntry.m_offset,
				1);

		}
	}


	void CommandList::TransitionResource(
		std::shared_ptr<re::Texture> texture, D3D12_RESOURCE_STATES toState, uint32_t subresourceIdx)
	{
		dx12::Texture::PlatformParams* texPlatParams =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>();

		m_accessedResources.emplace_back(AccessedResource{
			&texPlatParams->m_modificationFence, 
			IsWriteableState(toState) });

		// Handle the resource transition:
		ID3D12Resource* resource = texPlatParams->m_textureResource.Get();		

		// If we've already seen this resource before, we can record the transition now (as we prepend any initial
		// transitions when submitting the command list)	
		if (m_resourceStates.HasResourceState(resource, subresourceIdx)) // Is the subresource idx (or ALL) in our known states list?
		{
			const D3D12_RESOURCE_STATES currentKnownState = m_resourceStates.GetResourceState(resource, subresourceIdx);
			if (currentKnownState == toState)
			{
				return; // Before and after states must be different
			}
					
			const D3D12_RESOURCE_BARRIER barrier{
				.Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE, 
				.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
					.pResource = resource,
					.Subresource = subresourceIdx,
					.StateBefore = currentKnownState,
					.StateAfter = toState}
			};

			// TODO: Support batching of multiple barriers
			m_commandList->ResourceBarrier(1, &barrier);
		}

		// Record the new state after the transition:
		m_resourceStates.SetResourceState(resource, toState, subresourceIdx);
	}


	void CommandList::TransitionUAV(
		std::shared_ptr<re::Texture> texture, D3D12_RESOURCE_STATES toState, uint32_t subresourceIdx)
	{
		ID3D12Resource* resource =
			texture->GetPlatformParams()->As<dx12::Texture::PlatformParams*>()->m_textureResource.Get();

		// TODO: Should we use this to insert UAV barriers (separately from our transitions barriers)?
		// Reminder: If we go this route, we need to update the resource's modification fence value
		SEAssertF("TODO: Figure out what to do with this function");

		// If we've already seen this UAV before, we can record the transition now (as we prepend any initial
		// transitions when submitting the command list)	
		if (m_resourceStates.HasResourceState(resource, subresourceIdx))
		{
			const D3D12_RESOURCE_STATES currentState = m_resourceStates.GetResourceState(resource, subresourceIdx);
			if (currentState == toState)
			{
				return; // Before and after states must be different
			}
		
			const D3D12_RESOURCE_BARRIER barrier{
				.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
				.Flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE,
				.UAV = D3D12_RESOURCE_UAV_BARRIER{
					.pResource = resource}
			};

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


	void CommandList::DebugPrintResourceStates() const
	{
		LOG("-------------------\n\tCommandList \"%s\"\n\t-------------------", GetDebugName(m_commandList.Get()).c_str());
		m_resourceStates.DebugPrintResourceStates();
	}
}