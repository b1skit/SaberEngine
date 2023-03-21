// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "CommandList_DX12.h"
#include "Debug_DX12.h"

using Microsoft::WRL::ComPtr;


namespace
{
	using dx12::CheckHResult;


	ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type)
	{
		ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;

		HRESULT hr = device->CreateCommandAllocator(
			type, // Copy, compute, direct draw, etc
			IID_PPV_ARGS(&commandAllocator)); // REFIID/GUID (Globally-Unique IDentifier) for the command allocator
		// NOTE: IID_PPV_ARGS macro automatically supplies both the RIID & interface pointer

		CheckHResult(hr, "Failed to create command allocator");

		hr = commandAllocator->Reset();
		CheckHResult(hr, "Failed to reset command allocator");

		return commandAllocator;
	}
}


namespace dx12
{
	CommandList::CommandList(ID3D12Device2* device, D3D12_COMMAND_LIST_TYPE type)
		: m_gpuDescriptorHeaps(nullptr)
		, m_type(type)
	{
		m_commandAllocator = CreateCommandAllocator(device, type);

		// Create the command list:
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		HRESULT hr = device->CreateCommandList(
			deviceNodeMask,
			m_type,						// Direct draw/compute/copy/etc
			m_commandAllocator.Get(),	// The command allocator the command lists will be created on
			nullptr,					// Optional: Command list initial pipeline state
			IID_PPV_ARGS(&m_commandList)); // Command list interface REFIID/GUID, & destination for the populated command list
		// NOTE: IID_PPV_ARGS macro automatically supplies both the RIID & interface pointer

		CheckHResult(hr, "Failed to create command list");

		// Set the descriptor heaps (unless we're a copy command list):
		if (m_type != D3D12_COMMAND_LIST_TYPE::D3D12_COMMAND_LIST_TYPE_COPY)
		{
			// Create our GPU-visible descriptor heaps:
			m_gpuDescriptorHeaps = std::make_unique<GPUDescriptorHeap>(
				m_commandList.Get(),
				m_type,
				D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			// TODO: Handle Sampler descriptor heaps
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
		m_commandAllocator = nullptr;
	}


	void CommandList::ClearRTV(CD3DX12_CPU_DESCRIPTOR_HANDLE const& rtv, glm::vec4 const& clearColor)
	{
		m_commandList->ClearRenderTargetView(
			rtv,
			&clearColor.r,
			0,			// Number of rectangles in the proceeding D3D12_RECT ptr
			nullptr);	// Ptr to an array of rectangles to clear in the resource view. Clears entire view if null
	}


	void CommandList::ClearDepth(D3D12_CPU_DESCRIPTOR_HANDLE const& dsv, float clearColor)
	{
		m_commandList->ClearDepthStencilView(
			dsv,
			D3D12_CLEAR_FLAG_DEPTH,
			clearColor,
			0,
			0,
			nullptr);
	}


	void CommandList::TransitionResource(
		ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) const
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			resource,
			from,
			to);

		// TODO: Support batching of multiple barriers
		m_commandList->ResourceBarrier(1, &barrier);
	}

}