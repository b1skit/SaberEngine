// © 2022 Adam Badke. All rights reserved.
#include "CommandQueue_DX12.h"
#include "Debug_DX12.h"

using Microsoft::WRL::ComPtr;


namespace dx12
{
	CommandQueue::CommandQueue()
		: m_commandQueue(nullptr)
		, m_type(CommandList::D3DCommandListType(CommandList::CommandListType::CommandListType_Count))
		, m_deviceCache(nullptr)
		, m_fenceValue(0)
	{
	}


	void CommandQueue::Create(ComPtr<ID3D12Device2> displayDevice, CommandList::CommandListType type)
	{
		m_type = CommandList::D3DCommandListType(type);
		m_deviceCache = displayDevice; // Store a local copy, for convenience

		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
		switch (type)
		{
		case CommandList::CommandListType::Direct:
		case CommandList::CommandListType::Copy:
		{
			cmdQueueDesc.Type = m_type;
			cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
			cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // None, or Disable Timeout
			cmdQueueDesc.NodeMask = deviceNodeMask;
		}
		break;
		case CommandList::CommandListType::Compute: // TODO: Implement more command queue/list types
		case CommandList::CommandListType::Bundle:
		case CommandList::CommandListType::VideoDecode:
		case CommandList::CommandListType::VideoProcess:
		case CommandList::CommandListType::VideoEncode:
		default:
		{
			SEAssertF("Invalid or (currently) unsupported command list type");
		}
		break;
		}

		HRESULT hr = m_deviceCache->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_commandQueue));
		CheckHResult(hr, "Failed to create command queue");

		m_fence.Create(m_deviceCache);
	}


	void CommandQueue::Destroy()
	{
		m_fence.Destroy();
		m_commandQueue = nullptr;
		m_deviceCache = nullptr;
	}


	std::shared_ptr<dx12::CommandList> CommandQueue::GetCreateCommandList()
	{
		std::shared_ptr<dx12::CommandList> commandList = nullptr;
		if (!m_commandListPool.empty() && m_fence.IsFenceComplete(m_commandListPool.front()->GetFenceValue()))
		{
			commandList = m_commandListPool.front();
			m_commandListPool.pop();
		}
		else
		{
			commandList = std::make_shared<dx12::CommandList>(m_deviceCache.Get(), m_type);
		}

		commandList->Reset();

		return commandList;
	}


	uint64_t CommandQueue::Execute(uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists)
	{
		// Extract our raw pointers so we can execute them in a single call
		std::vector<ID3D12CommandList*> commandListPtrs;
		commandListPtrs.reserve(numCmdLists);

		// Get our raw command list pointers, and close them before they're executed
		for (uint32_t i = 0; i < numCmdLists; i++)
		{
			SEAssert("Command list type does not match command queue type", cmdLists[i]->GetType() == m_type);

			cmdLists[i]->Close(); 
			commandListPtrs.emplace_back(cmdLists[i]->GetD3DCommandList());
		}

		// Execute the command lists:
		m_commandQueue->ExecuteCommandLists(numCmdLists, &commandListPtrs[0]);

		// Fence value for when the command list's internal command allocator will be available for reuse
		const uint64_t fenceVal = Signal();

		// Return our command list(s) to the pool:
		for (uint32_t i = 0; i < numCmdLists; i++)
		{
			cmdLists[i]->SetFenceValue(fenceVal);			
			m_commandListPool.emplace(cmdLists[i]);
			cmdLists[i] = nullptr; // We don't want the caller retaining access to a command list in our pool
		}

		return fenceVal;
	}


	uint64_t CommandQueue::Signal()
	{
		return m_fence.Signal(m_commandQueue, m_fenceValue); // m_fenceValue will be incremented
	}


	void CommandQueue::WaitForGPU(uint64_t fenceValue)
	{
		m_fence.WaitForGPU(fenceValue);
	}


	void CommandQueue::Flush()
	{
		const uint64_t fenceValueForSignal = m_fence.Signal(m_commandQueue, m_fenceValue);
		m_fence.WaitForGPU(fenceValueForSignal);
	}


	void CommandQueue::GPUWait(uint64_t fenceValue)
	{
		HRESULT hr = m_commandQueue->Wait(m_fence.GetD3DFence(), fenceValue);
	}
}