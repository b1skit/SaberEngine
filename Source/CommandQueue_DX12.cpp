// © 2022 Adam Badke. All rights reserved.
#include "CommandList_DX12.h"
#include "CommandQueue_DX12.h"
#include "Debug_DX12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	CommandQueue_DX12::CommandQueue_DX12()
		: m_commandQueue(nullptr)
		, m_fenceValue(0)
	{
	}


	void CommandQueue_DX12::Create(ComPtr<ID3D12Device2> displayDevice, D3D12_COMMAND_LIST_TYPE type)
	{
		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
		cmdQueueDesc.Type = type; // Direct, compute, copy, etc
		cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // None, or Disable Timeout
		cmdQueueDesc.NodeMask = deviceNodeMask;

		HRESULT hr = displayDevice->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_commandQueue));
		CheckHResult(hr, "Failed to create command queue");

		m_fence.Create(displayDevice);
	}


	void CommandQueue_DX12::Destroy()
	{
		m_fence.Destroy();
	}


	void CommandQueue_DX12::Execute(uint32_t numCmdLists, ID3D12CommandList* const* cmdLists)
	{
		m_commandQueue->ExecuteCommandLists(numCmdLists, cmdLists);
	}


	uint64_t CommandQueue_DX12::Signal()
	{
		return m_fence.Signal(m_commandQueue, m_fenceValue);
	}


	void CommandQueue_DX12::WaitForGPU(uint64_t fenceValue)
	{
		m_fence.WaitForGPU(fenceValue);
	}


	void CommandQueue_DX12::Flush()
	{
		const uint64_t fenceValueForSignal = m_fence.Signal(m_commandQueue, m_fenceValue);
		m_fence.WaitForGPU(fenceValueForSignal);
	}


	//Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandQueue_DX12::GetCommandList()
	//{

	//}
}