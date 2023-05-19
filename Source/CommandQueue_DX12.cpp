// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Context_DX12.h"
#include "CommandQueue_DX12.h"
#include "Debug_DX12.h"

using Microsoft::WRL::ComPtr;


namespace
{
	using dx12::LocalResourceStateTracker;
	using dx12::GlobalResourceState;


	std::vector<std::shared_ptr<dx12::CommandList>> InsertPendingBarrierCommandLists(
		uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists, dx12::CommandQueue& commandQueue)
	{
		constexpr uint32_t k_allSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		// Extract our raw pointers so we can execute them in a single call
		auto AddBarrier = [](
			ID3D12Resource* resource,
			D3D12_RESOURCE_STATES beforeState,
			D3D12_RESOURCE_STATES afterState,
			uint32_t subresourceIdx,
			std::vector<CD3DX12_RESOURCE_BARRIER>& barriers,
			D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE
			)
		{
			barriers.emplace_back(CD3DX12_RESOURCE_BARRIER::Transition(
				resource,
				beforeState,
				afterState,
				subresourceIdx,
				flags));
		};

		// Construct our transition barrier command lists:
		std::vector<std::shared_ptr<dx12::CommandList>> finalCommandLists;
		
		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		dx12::GlobalResourceStateTracker& globalResourceStates = ctxPlatParams->m_globalResourceStates;

		// Manually patch the barriers for each command list:
		std::lock_guard<std::mutex> barrierLock(globalResourceStates.GetGlobalStatesMutex());

		for (uint32_t cmdListIdx = 0; cmdListIdx < numCmdLists; cmdListIdx++)
		{
			std::vector<CD3DX12_RESOURCE_BARRIER> barriers;

			LocalResourceStateTracker const& localResourceTracker = cmdLists[cmdListIdx]->GetLocalResourceStates();

			// Handle pending transitions for the current command list:
			for (auto const& currentPending : localResourceTracker.GetPendingResourceStates())
			{
				ID3D12Resource* resource = currentPending.first;
				GlobalResourceState const& globalStates = globalResourceStates.GetResourceState(resource);
				dx12::LocalResourceState const& pendingStates = currentPending.second;

				// 1) Handle pending "ALL" transitions: Must transition each subresource from its (potentially) 
				// unique before state
				if (pendingStates.HasSubresourceRecord(k_allSubresources))
				{
					const D3D12_RESOURCE_STATES afterState = pendingStates.GetState(k_allSubresources);

					// We only have a single before/after state for all subresources:
					if (pendingStates.GetStates().size() == 1 && globalStates.GetStates().size() == 1)
					{
						SEAssert("Global state with a single entry should have an all subresources entry",
							globalStates.HasSubresourceRecord(k_allSubresources));
						const D3D12_RESOURCE_STATES beforeState = globalStates.GetState(k_allSubresources);

						AddBarrier(resource, beforeState, afterState, k_allSubresources, barriers);
					}
					else // We have multiple before/after subresource states:
					{
						for (uint32_t subresourceIdx = 0; subresourceIdx < globalStates.GetNumSubresources(); subresourceIdx++)
						{
							const D3D12_RESOURCE_STATES beforeState = globalStates.GetState(subresourceIdx);
							if (beforeState != afterState)
							{
								AddBarrier(resource, beforeState, afterState, subresourceIdx, barriers);
							}
						}
					}

					// Finally, update the global resource state. TODO: This may be inefficient, as we're
					// potentially doing an uncessary transition to the ALL state for some subresources that had
					// a pending individual transition written before the ALL state was written
					globalResourceStates.SetResourceState(resource, afterState, k_allSubresources);
				}

				// 2) Handle any per-subresource pending transitions:
				for (auto const& localSRState : pendingStates.GetStates())
				{
					const uint32_t subresourceIdx = localSRState.first;
					if (subresourceIdx == k_allSubresources)
					{
						continue; // Already handled above
					}

					const D3D12_RESOURCE_STATES beforeState = globalStates.GetState(subresourceIdx);
					const D3D12_RESOURCE_STATES afterState = localSRState.second;
					if (beforeState == afterState)
					{
						continue; // No transition needed
					}

					AddBarrier(resource, beforeState, afterState, subresourceIdx, barriers);

					// Update the global state:
					globalResourceStates.SetResourceState(resource, afterState, subresourceIdx);
				}
			}

			// Add the transition barriers to a command list, if we actually made any:
			if (!barriers.empty())
			{
				std::shared_ptr<dx12::CommandList> barrierCommandList = commandQueue.GetCreateCommandList();

				barrierCommandList->GetD3DCommandList()->ResourceBarrier(
					static_cast<uint32_t>(barriers.size()),
					&barriers[0]);

				finalCommandLists.emplace_back(barrierCommandList);
			}

			// Add the original command list:
			finalCommandLists.emplace_back(cmdLists[cmdListIdx]);
			cmdLists[cmdListIdx] = nullptr; // We don't want the caller retaining access to a command list in our pool
		}

		return finalCommandLists;
	}
}
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
		std::vector<std::shared_ptr<dx12::CommandList>> finalCommandLists = 
			std::move(InsertPendingBarrierCommandLists(numCmdLists, cmdLists, *this));

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