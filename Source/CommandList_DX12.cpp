// © 2022 Adam Badke. All rights reserved.
#include "CommandList_DX12.h"
#include "Debug_DX12.h"


namespace dx12
{
	using Microsoft::WRL::ComPtr;


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
		hr = m_commandList->Close();
		CheckHResult(hr, "Failed to close command list");
	}
}