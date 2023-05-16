// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Context_DX12.h"
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

		std::string fenceEventName;

		D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
		cmdQueueDesc.Type		= m_type;
		cmdQueueDesc.Priority	= D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmdQueueDesc.Flags		= D3D12_COMMAND_QUEUE_FLAG_NONE; // None, or Disable Timeout
		cmdQueueDesc.NodeMask	= deviceNodeMask;

		switch (type)
		{
		case CommandList::CommandListType::Direct:
		{
			fenceEventName = "Direct queue fence event";
		}
		break;
		case CommandList::CommandListType::Copy:
		{
			fenceEventName = "Copy queue fence event";
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

		m_fence.Create(m_deviceCache, fenceEventName.c_str());
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
		

		// Construct our transition barrier command lists:
		std::vector<std::shared_ptr<dx12::CommandList>> finalCommandLists;
		{
			dx12::Context::PlatformParams* ctxPlatParams = 
				re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();
			
			dx12::GlobalResourceStateTracker& globalResourceStates = ctxPlatParams->m_globalResourceStates;
			
			// Manually patch the barriers for each command list:
			std::lock_guard<std::mutex> barrierLock(globalResourceStates.GetGlobalStatesMutex());

			for (uint32_t i = 0; i < numCmdLists; i++)
			{
				uint32_t numBarriers = 0;
				std::shared_ptr<dx12::CommandList> barrierCommandList = GetCreateCommandList();

				LocalResourceStateTracker const& localResourceStates = cmdLists[i]->GetLocalResourceStates();

				for (auto const& currentResourceEntry : localResourceStates.GetPendingResourceStates())
				{
					ID3D12Resource* resource = currentResourceEntry.first;
					for (auto const& currentState : currentResourceEntry.second.GetStates())
					{
						const uint32_t subresourceIdx = currentState.first;
						const D3D12_RESOURCE_STATES subresourceState = currentState.second;

						if (!globalResourceStates.ResourceStateMatches(resource,
							subresourceState,
							subresourceIdx))
						{
							// Record a barrier, and update the global state:
							CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
								resource,
								globalResourceStates.GetResourceState(resource).GetState(subresourceIdx),
								subresourceState,
								subresourceIdx,
								D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE);

							// TODO: Support batching of multiple barriers
							barrierCommandList->GetD3DCommandList()->ResourceBarrier(1, &barrier);

							globalResourceStates.SetResourceState(resource, subresourceState, subresourceIdx);
							numBarriers++;
						}
					}
				}

				// Prepend the transition barrier command list, if we actually made any transitions:
				if (numBarriers > 0)
				{
					finalCommandLists.emplace_back(barrierCommandList);
				}

				// Add the original command list:
				finalCommandLists.emplace_back(cmdLists[i]);
				cmdLists[i] = nullptr; // We don't want the caller retaining access to a command list in our pool
			}
		}

		// Get our raw command list pointers, and close them before they're executed
		std::vector<ID3D12CommandList*> commandListPtrs;
		commandListPtrs.reserve(finalCommandLists.size());
		for (uint32_t i = 0; i < finalCommandLists.size(); i++)
		{
			SEAssert("Command list type does not match command queue type", finalCommandLists[i]->GetType() == m_type);

			finalCommandLists[i]->Close();
			commandListPtrs.emplace_back(finalCommandLists[i]->GetD3DCommandList());
		}

		// Execute the command lists:
		m_commandQueue->ExecuteCommandLists(static_cast<uint32_t>(finalCommandLists.size()), &commandListPtrs[0]);

		// Insert a fence for when the command list's internal command allocator will be available for reuse
		const uint64_t fenceVal = GPUSignal();

		// Return our command list(s) to the pool:
		for (uint32_t i = 0; i < finalCommandLists.size(); i++)
		{
			finalCommandLists[i]->SetFenceValue(fenceVal);

			m_commandListPool.push(finalCommandLists[i]);

			finalCommandLists[i] = nullptr;
		}

		return fenceVal;
	}


	uint64_t CommandQueue::CPUSignal()
	{
		// Updates the fence to the specified value from the CPU side
		m_fence.CPUSignal(++m_fenceValue); // Note: First fenceValueForSignal == 1
		return m_fenceValue;
	}


	void CommandQueue::CPUWait(uint64_t fenceValue) const
	{
		// CPU waits until fence is set to the given value
		m_fence.CPUWait(fenceValue);
	}


	void CommandQueue::Flush()
	{
		const uint64_t fenceValueForSignal = GPUSignal();
		m_fence.CPUWait(fenceValueForSignal);
	}


	uint64_t CommandQueue::GPUSignal()
	{
		// Update the fence value from the GPU side
		HRESULT hr = m_commandQueue->Signal(m_fence.GetD3DFence(), ++m_fenceValue);
		CheckHResult(hr, "Command queue failed to issue GPU signal");

		return m_fenceValue;
	}


	void CommandQueue::GPUWait(uint64_t fenceValue) const
	{
		// Queue a GPU wait (GPU waits until the specified fence reaches/exceeds the fenceValue), and returns immediately
		HRESULT hr = m_commandQueue->Wait(m_fence.GetD3DFence(), fenceValue);
		CheckHResult(hr, "Command queue failed to issue GPU wait");
	}
}