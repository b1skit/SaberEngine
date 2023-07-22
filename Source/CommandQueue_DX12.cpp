// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Context_DX12.h"
#include "CommandQueue_DX12.h"
#include "Debug_DX12.h"

using Microsoft::WRL::ComPtr;

//#define DEBUG_RESOURCE_STATES

namespace
{
	using dx12::LocalResourceStateTracker;
	using dx12::GlobalResourceState;


#if defined(DEBUG_RESOURCE_STATES)
	void DebugPrintBarrier(
		ID3D12Resource* resource, 
		D3D12_RESOURCE_STATES beforeState,
		D3D12_RESOURCE_STATES afterState, 
		uint32_t subresourceIdx)
	{
		LOG("BARRIER: Resource \"%s\"\n\tSubresource #%s: From: %s To: %s",
			dx12::GetDebugName(resource).c_str(),
			(subresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES ? "ALL" : std::to_string(subresourceIdx).c_str()),
			dx12::GetResourceStateAsStr(beforeState),
			dx12::GetResourceStateAsStr(afterState));
	}
#endif


	std::vector<std::shared_ptr<dx12::CommandList>> InsertPendingBarrierCommandLists(
		uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists, dx12::CommandQueue& commandQueue)
	{
		constexpr uint32_t k_allSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		// Extract our raw pointers so we can execute them in a single call
		auto AddTransitionBarrier = [](
			ID3D12Resource* resource,
			D3D12_RESOURCE_STATES beforeState,
			D3D12_RESOURCE_STATES afterState,
			uint32_t subresourceIdx,
			std::vector<D3D12_RESOURCE_BARRIER>& barriers,
			D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE)
		{
			// TODO: This check is duplicated in CommandList::TransitionResource
			// All barriers should be set in a single place
			if (beforeState == afterState)
			{
				return;
			}
			barriers.emplace_back(D3D12_RESOURCE_BARRIER{
				.Type = D3D12_RESOURCE_BARRIER_TYPE::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
				.Flags = flags,
				.Transition = D3D12_RESOURCE_TRANSITION_BARRIER{
					.pResource = resource,
					.Subresource = subresourceIdx,
					.StateBefore = beforeState,
					.StateAfter = afterState}
				});
#if defined(DEBUG_RESOURCE_STATES)
			DebugPrintBarrier(resource, beforeState, afterState, subresourceIdx);
#endif
		};

		// Construct our transition barrier command lists:
		std::vector<std::shared_ptr<dx12::CommandList>> finalCommandLists;
		
		dx12::Context::PlatformParams* ctxPlatParams =
			re::RenderManager::Get()->GetContext().GetPlatformParams()->As<dx12::Context::PlatformParams*>();

		dx12::GlobalResourceStateTracker& globalResourceStates = ctxPlatParams->m_globalResourceStates;

		// Manually patch the barriers for each command list:
		std::lock_guard<std::mutex> barrierLock(globalResourceStates.GetGlobalStatesMutex());
		// TODO: This logic should be a part of the GlobalResourceState
		// -> Should be responsible for locking/unlocking it's own mutex


#if defined(DEBUG_RESOURCE_STATES)
		LOG("---------- InsertPendingBarrierCommandLists ----------");
		globalResourceStates.DebugPrintResourceStates();
#endif

		for (uint32_t cmdListIdx = 0; cmdListIdx < numCmdLists; cmdListIdx++)
		{
			std::vector<D3D12_RESOURCE_BARRIER> barriers;

			LocalResourceStateTracker const& localResourceTracker = cmdLists[cmdListIdx]->GetLocalResourceStates();

#if defined(DEBUG_RESOURCE_STATES)
			cmdLists[cmdListIdx]->DebugPrintResourceStates();;
#endif

			// Handle pending transitions for the current command list:
			for (auto const& currentPending : localResourceTracker.GetPendingResourceStates())
			{
				ID3D12Resource* resource = currentPending.first;
				dx12::LocalResourceState const& pendingStates = currentPending.second;
				GlobalResourceState const& globalState = globalResourceStates.GetResourceState(resource);

				for (auto const& pendingState : pendingStates.GetStates())
				{
					const uint32_t subresourceIdx = pendingState.first;
					if (subresourceIdx == k_allSubresources)
					{
						continue; // We'll handle the ALL state last
					}

					const D3D12_RESOURCE_STATES beforeState = globalState.GetState(subresourceIdx);
					const D3D12_RESOURCE_STATES afterState = pendingState.second;
					if (beforeState != afterState)
					{
						AddTransitionBarrier(resource, beforeState, afterState, subresourceIdx, barriers);
						globalResourceStates.SetResourceState(resource, afterState, subresourceIdx);
					}
				}

				if (pendingStates.HasSubresourceRecord(k_allSubresources))
				{
					const D3D12_RESOURCE_STATES afterState = pendingStates.GetState(k_allSubresources);
					bool insertedTransition = false;
					for (uint32_t subresourceIdx = 0; subresourceIdx < globalState.GetNumSubresources(); subresourceIdx++)
					{
						const D3D12_RESOURCE_STATES beforeState = globalState.GetState(subresourceIdx);
						if (beforeState != afterState)
						{
							AddTransitionBarrier(resource, beforeState, afterState, subresourceIdx, barriers);
							insertedTransition = true;
						}
					}
					if (!insertedTransition)
					{
						AddTransitionBarrier(resource, globalState.GetState(k_allSubresources), afterState, k_allSubresources, barriers);
					}
					globalResourceStates.SetResourceState(resource, afterState, k_allSubresources);
				}
			}

			// Finally, update the global state from the known final local states:
			for (auto const& currentknown : localResourceTracker.GetKnownResourceStates())
			{
				ID3D12Resource* resource = currentknown.first;
				dx12::LocalResourceState const& knownStates = currentknown.second;
				GlobalResourceState const& globalState = globalResourceStates.GetResourceState(resource);

				// Set the ALL state first:
				if (knownStates.HasSubresourceRecord(k_allSubresources))
				{
					globalResourceStates.SetResourceState(
						resource, knownStates.GetState(k_allSubresources), k_allSubresources);
				}

				for (auto const& knownState : currentknown.second.GetStates())
				{
					const uint32_t subresourceIdx = knownState.first;
					if (subresourceIdx == k_allSubresources)
					{
						continue; // We handled the ALL state fisrt
					}

					globalResourceStates.SetResourceState(resource, knownStates.GetState(subresourceIdx), subresourceIdx);
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


#if defined(DEBUG_RESOURCE_STATES)
		globalResourceStates.DebugPrintResourceStates();
		LOG("--------------end--------------");
#endif

		return finalCommandLists;
	}
}


namespace dx12
{
	CommandQueue::CommandQueue()
		: m_commandQueue(nullptr)
		, m_type(CommandList::GetD3DCommandListType(CommandList::CommandListType::CommandListType_Invalid))
		, m_deviceCache(nullptr)
		, m_fenceValue(0)
	{
	}


	void CommandQueue::Create(ComPtr<ID3D12Device2> displayDevice, CommandList::CommandListType type)
	{
		m_type = CommandList::GetD3DCommandListType(type);
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
		case CommandList::CommandListType::Compute: 
		{
			fenceEventName = "Compute queue fence event";
		}
		break;
		case CommandList::CommandListType::Bundle: // TODO: Implement more command queue/list types
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
		m_fenceValue = 0;
		m_commandQueue = nullptr;
		m_deviceCache = nullptr;
		
		// Swap our queue with an empty version to clear it
		std::queue<std::shared_ptr<dx12::CommandList>> emptyCommandListPool;
		std::swap(m_commandListPool, emptyCommandListPool);
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
		// Prepend pending resource barrier command lists to the list of command lists we're executing
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
		m_commandQueue->ExecuteCommandLists(static_cast<uint32_t>(commandListPtrs.size()), commandListPtrs.data());

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