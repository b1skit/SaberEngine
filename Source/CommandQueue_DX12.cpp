// © 2022 Adam Badke. All rights reserved.
#include "CommandQueue_DX12.h"
#include "Debug_DX12.h"


namespace dx12
{
	using Microsoft::WRL::ComPtr;


	void CommandQueue::Create(ComPtr<ID3D12Device2> displayDevice, D3D12_COMMAND_LIST_TYPE type)
	{
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
		cmdQueueDesc.Type = type; // Direct, compute, copy, etc
		cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // None, or Disable Timeout
		cmdQueueDesc.NodeMask = deviceNodeMask;

		HRESULT hr = displayDevice->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_commandQueue));
		CheckHResult(hr, "Failed to create command queue");
	}
}