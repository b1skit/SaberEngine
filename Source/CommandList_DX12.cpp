// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "CommandList_DX12.h"
#include "Debug_DX12.h"


namespace dx12
{
	using Microsoft::WRL::ComPtr;

	CommandList_DX12::CommandList_DX12()
		: m_commandList(nullptr)
	{
	}


	void CommandList_DX12::Create(Microsoft::WRL::ComPtr<ID3D12Device2> device,
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAllocator,
		D3D12_COMMAND_LIST_TYPE type)
	{
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		HRESULT hr = device->CreateCommandList(
			deviceNodeMask,
			type, // Direct draw/compute/copy/etc
			cmdAllocator.Get(), // The command allocator the command lists will be created on
			nullptr,  // Optional: Command list initial pipeline state
			IID_PPV_ARGS(&m_commandList)); // REFIID/GUID of the command list interface, & destination for the populated command list
		// NOTE: IID_PPV_ARGS macro automatically supplies both the RIID & interface pointer

		CheckHResult(hr, "Failed to create command list");

		// Note: Command lists are created in the recording state by default. The render loop resets the command list,
		// which requires the command list to be closed. So, we pre-close new command lists so they're ready to be reset 
		// before recording
		Close();
	}


	void CommandList_DX12::AddResourceBarrier(uint8_t numBarriers, CD3DX12_RESOURCE_BARRIER* const barriers)
	{
		m_commandList->ResourceBarrier(numBarriers, barriers);
	}


	void CommandList_DX12::Close()
	{
		HRESULT hr = m_commandList->Close();
		CheckHResult(hr, "Failed to close command list");
	}


	void CommandList_DX12::Reset(ComPtr<ID3D12CommandAllocator> commandAllocator, ID3D12PipelineState* pso)
	{
		// Note: pso is optional; A dummy initial PSO is set if nullptr
		m_commandList->Reset(commandAllocator.Get(), pso);
	}


	void CommandList_DX12::ClearRTV(
		CD3DX12_CPU_DESCRIPTOR_HANDLE& rtv, glm::vec4 const& clearColor, uint8_t numRects, D3D12_RECT const* rects)
	{
		m_commandList->ClearRenderTargetView(
			rtv, // Descriptor we're clearning
			&clearColor.x,
			0, // Number of rectangles in the proceeding D3D12_RECT ptr
			nullptr); // Ptr to an array of rectangles to clear in the resource view. Clears the entire view if null
	}
}