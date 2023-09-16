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
			dx12::GetResourceStateAsCStr(beforeState),
			dx12::GetResourceStateAsCStr(afterState));
	}
#endif


	constexpr bool NeedsCommonTransition(
		D3D12_RESOURCE_STATES currentGlobalState, 
		dx12::CommandListType srcCmdListType, 
		dx12::CommandListType dstCmdListType)
	{
		// This function is based on the information on these pages:
		// https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html#command-queue-layout-compatibility
		// https://microsoft.github.io/DirectX-Specs/d3d/CPUEfficiency.html#state-support-by-command-list-type

		SEAssert("Invalid state for transition", 
			currentGlobalState != D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

		SEAssert("We should genenerally avoid this state. See this page for more info: "
			"https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states", 
			currentGlobalState != D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ);

		// If the previous and current command list type are the same, we know they'll support the same transition
		// types. No need to go to COMMON between them
		if (srcCmdListType == dstCmdListType)
		{
			return false;
		}

		// Check if the destination command list supports the resource state type. If it does, no need to transition to
		// common on another command queue/command list type.
		// Note: COPY states are considered different for direct/compute vs copy queues, so we explicitely require a
		// transition to COMMON here
		switch (dstCmdListType)
		{
		case dx12::CommandListType::Direct:
		{
			switch (currentGlobalState)
			{
				// Already in these states? The direct queue can handle it, no need to go to COMMON
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDEX_BUFFER:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_READ:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_STREAM_OUT:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RESOLVE_DEST:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RESOLVE_SOURCE:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE:
				return false;
			default: return true; // Everything else needs to be in COMMON first
			}
		}
		break;
		case dx12::CommandListType::Compute:
		{
			switch (currentGlobalState)
			{
				// Already in these states? The compute queue can handle it, no need to go to COMMON
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
			case D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
				return false; 
			default: return true; // Everything else needs to be in COMMON first
			}
		}
		break;
		case dx12::CommandListType::Copy:
		{
			// The copy queue only supports the COPY_SOURCE and COPY_DEST states, and they're considered different to
			// the COPY_SOURCE/COPY_DEST states on direct and compute queues. Thus, always require a resource is in the
			// COMMON state before it's used on a copy queue for the first time
			return true;
		}
		break;
		case dx12::CommandListType::Bundle:
		case dx12::CommandListType::VideoDecode:
		case dx12::CommandListType::VideoProcess:
		case dx12::CommandListType::VideoEncode:
		default: SEAssertF("Invalid/currently unsupported command list type");
		}
		return true; // This should never happen
	}


	// Extract our raw pointers so we can execute them in a single call
	void AddTransitionBarrier(
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
}


namespace dx12
{
	CommandQueue::CommandQueue()
		: m_commandQueue(nullptr)
		, m_type(CommandListType::CommandListType_Invalid)
		, m_d3dType(D3D12_COMMAND_LIST_TYPE_NONE)
		, m_deviceCache(nullptr)
		, m_fenceValue(0)
		, m_typeFenceBitMask(0)
		, m_isCreated(false)
	{
	}


	void CommandQueue::Create(ComPtr<ID3D12Device2> displayDevice, dx12::CommandListType type)
	{
		m_type = type;
		m_d3dType = CommandList::TranslateToD3DCommandListType(type);
		m_deviceCache = displayDevice; // Store a local copy, for convenience

		constexpr uint32_t deviceNodeMask = 0; // Always 0: We don't (currently) support multiple GPUs

		std::string fenceEventName;

		D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
		cmdQueueDesc.Type		= m_d3dType;
		cmdQueueDesc.Priority	= D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmdQueueDesc.Flags		= D3D12_COMMAND_QUEUE_FLAG_NONE; // None, or Disable Timeout
		cmdQueueDesc.NodeMask	= deviceNodeMask;

		switch (type)
		{
		case CommandListType::Direct:
		{
			fenceEventName = "Direct queue fence event";
		}
		break;
		case CommandListType::Copy:
		{
			fenceEventName = "Copy queue fence event";
		}
		break;
		case CommandListType::Compute: 
		{
			fenceEventName = "Compute queue fence event";
		}
		break;
		case CommandListType::Bundle: // TODO: Implement more command queue/list types
		case CommandListType::VideoDecode:
		case CommandListType::VideoProcess:
		case CommandListType::VideoEncode:
		default:
		{
			SEAssertF("Invalid or (currently) unsupported command list type");
		}
		break;
		}

		HRESULT hr = m_deviceCache->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_commandQueue));
		CheckHResult(hr, "Failed to create command queue");

		m_fence.Create(m_deviceCache, fenceEventName.c_str());
		m_typeFenceBitMask = dx12::Fence::GetCommandListTypeFenceMaskBits(type);
		m_fenceValue = m_typeFenceBitMask; // Fence value effectively starts at 0, with the type bits set

		SEAssert("The fence value should be 0 after removing the command queue type bits", 
			dx12::Fence::GetRawFenceValue(m_fenceValue) == 0);

		m_isCreated = true;
	}


	void CommandQueue::Destroy()
	{
		if (!m_isCreated)
		{
			return;
		}
		m_isCreated = false;

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
		if (!m_commandListPool.empty() && m_fence.IsFenceComplete(m_commandListPool.front()->GetReuseFenceValue()))
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


	// Command lists can only transition resources to/from states compatible with their type. Thus, we must first 
	// transition any resources in incompatible states back to common on the appropriate command queue type.
	// Note: We're recording/submitting command lists to different command queue types here: This should be done 
	// single-threaded, like all other command list submissions
	void CommandQueue::TransitionIncompatibleResourceStatesToCommon(
		uint32_t numCmdLists,
		std::shared_ptr<dx12::CommandList>* cmdLists)
	{
		dx12::Context* context = re::Context::GetAs<dx12::Context*>();

		dx12::CommandQueue& directQueue = context->GetCommandQueue(dx12::CommandListType::Direct);
		std::shared_ptr<dx12::CommandList> directCmdList = nullptr;
		std::vector<D3D12_RESOURCE_BARRIER> directBarriers; // TODO: Set a reasonable reservation amount?

		dx12::CommandQueue& computeQueue = context->GetCommandQueue(dx12::CommandListType::Compute);
		std::shared_ptr<dx12::CommandList> computeCmdList = nullptr;
		std::vector<D3D12_RESOURCE_BARRIER> computeBarriers; // TODO: Set a reasonable reservation amount?

		dx12::CommandQueue& copyQueue = context->GetCommandQueue(dx12::CommandListType::Copy);
		std::shared_ptr<dx12::CommandList> copyCmdList = nullptr;
		std::vector<D3D12_RESOURCE_BARRIER> copyBarriers; // TODO: Set a reasonable reservation amount?

		// Note: We're going to independently submit COMMON resource transitions on command lists executed by the same
		// queue a resource was last used on. Thus, we don't need to fence on the previous work in these queues

		// Check the global state of every *pending* resource in the command lists we're about to submit:
		dx12::GlobalResourceStateTracker& globalResourceStates = context->GetGlobalResourceStates();
		{
			std::lock_guard<std::mutex> barrierLock(globalResourceStates.GetGlobalStatesMutex());

			for (uint32_t cmdListIdx = 0; cmdListIdx < numCmdLists; cmdListIdx++)
			{
				LocalResourceStateTracker const& localResourceTracker = cmdLists[cmdListIdx]->GetLocalResourceStates();

				for (auto const& currentPending : localResourceTracker.GetPendingResourceStates())
				{
					ID3D12Resource* currentResource = currentPending.first;
					dx12::GlobalResourceState const& globalResourceState =
						globalResourceStates.GetResourceState(currentResource);
					const dx12::CommandListType srcCmdListType = globalResourceState.GetLastCommandListType();

					if (srcCmdListType == dx12::CommandListType::CommandListType_Invalid)
					{
						continue; // Resource not used yet
					}

					for (auto const& pendingState : currentPending.second.GetStates())
					{
						const uint32_t pendingSubresource = pendingState.first;
						const D3D12_RESOURCE_STATES globalD3DState = globalResourceState.GetState(pendingSubresource);
						if (NeedsCommonTransition(globalD3DState, srcCmdListType, m_type))
						{
							// Transition the resource from its current global state to common:
							std::vector<D3D12_RESOURCE_BARRIER>* targetBarriers = nullptr;
							uint64_t nextFenceValue = 0;
							switch (srcCmdListType)
							{
							case dx12::CommandListType::Direct:
							{
								if (directCmdList == nullptr)
								{
									directCmdList = directQueue.GetCreateCommandList();
								}
								targetBarriers = &directBarriers;
								nextFenceValue = directQueue.GetNextFenceValue();
							}
							break;
							case dx12::CommandListType::Compute:
							{
								if (computeCmdList == nullptr)
								{
									computeCmdList = computeQueue.GetCreateCommandList();
								}
								targetBarriers = &computeBarriers;
								nextFenceValue = computeQueue.GetNextFenceValue();
							}
							break;
							case dx12::CommandListType::Copy:
							{
								if (copyCmdList == nullptr)
								{
									copyCmdList = copyQueue.GetCreateCommandList();
								}
								targetBarriers = &copyBarriers;
								nextFenceValue = copyQueue.GetNextFenceValue();
							}
							break;
							default:
								SEAssertF("Invalid/unsupported command list type");
							}

							AddTransitionBarrier(
								currentResource,
								globalD3DState,
								D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
								pendingSubresource,
								*targetBarriers);

							globalResourceStates.SetResourceState(
								currentResource,
								D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
								pendingSubresource,
								nextFenceValue);
						}
					}
				}
			}
		}

		// Execute our transitions to COMMON, and have our main command queue wait on GPU fences to ensure the 
		// transitions are complete before proceeding
		if (!directBarriers.empty())
		{
			directCmdList->GetD3DCommandList()->ResourceBarrier(
				static_cast<uint32_t>(directBarriers.size()),
				directBarriers.data());

			const uint64_t directBarrierFence = directQueue.ExecuteInternal({ directCmdList }, 0);

			GPUWait(directQueue.GetFence(), directBarrierFence);
		}
		if (!computeBarriers.empty())
		{
			computeCmdList->GetD3DCommandList()->ResourceBarrier(
				static_cast<uint32_t>(computeBarriers.size()),
				computeBarriers.data());

			const uint64_t computeBarrierFence = computeQueue.ExecuteInternal({ computeCmdList }, 0);

			GPUWait(computeQueue.GetFence(), computeBarrierFence);
		}
		if (!copyBarriers.empty())
		{
			copyCmdList->GetD3DCommandList()->ResourceBarrier(
				static_cast<uint32_t>(copyBarriers.size()),
				copyBarriers.data());

			const uint64_t copyBarrierFence = copyQueue.ExecuteInternal({ copyCmdList }, 0);

			GPUWait(copyQueue.GetFence(), copyBarrierFence);
		}
	}


	std::vector<std::shared_ptr<dx12::CommandList>> CommandQueue::PrependBarrierCommandListsAndWaits(
		uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists)
	{
		constexpr uint32_t k_allSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		// Construct our transition barrier command lists:
		std::vector<std::shared_ptr<dx12::CommandList>> finalCommandLists;

		// We'll store the highest modification fence values seen for resources accessed by the submitted command lists,
		// so we can insert GPU waits before executing our final batch of command lists
		std::array<uint64_t, dx12::CommandListType::CommandListType_Count> maxModificationFences;
		memset(&maxModificationFences, 0, sizeof(uint64_t) * dx12::CommandListType::CommandListType_Count);

		dx12::Context* context = re::Context::GetAs<dx12::Context*>();
		dx12::GlobalResourceStateTracker& globalResourceStates = context->GetGlobalResourceStates();

		// Manually patch the barriers for each command list:
		{
			std::lock_guard<std::mutex> barrierLock(globalResourceStates.GetGlobalStatesMutex());
			// TODO: This logic should be a part of the GlobalResourceState
			// -> Should be responsible for locking/unlocking it's own mutex


#if defined(DEBUG_RESOURCE_STATES)
			LOG("---------- PrependBarrierCommandListsAndWaits ----------");
			globalResourceStates.DebugPrintResourceStates();
#endif

			const uint64_t nextFenceVal = GetNextFenceValue();

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

					// Cache the global modification value: We'll GPU wait on the most recent modification fence
					const dx12::CommandListType lastGlobalCmdListType = globalState.GetLastCommandListType();
					if (lastGlobalCmdListType != dx12::CommandListType::CommandListType_Count) // Has it ever been used on a command list?
					{
						maxModificationFences[lastGlobalCmdListType] = std::max(
							globalState.GetLastModificationFenceValue(),
							maxModificationFences[lastGlobalCmdListType]);
					}

					const uint32_t numSubresources = globalState.GetNumSubresources();

					uint32_t numSubresourcesTransitioned = 0;
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
							globalResourceStates.SetResourceState(resource, afterState, subresourceIdx, nextFenceVal);
							numSubresourcesTransitioned++;
						}
					}

					// Note: There is an edge case where we could individually add each subresource to the pending list, and
					// then add an "ALL" transition which would be (incorrectly) added to the pending list. So, we handle
					// that here (as it makes the bookkeeping much simpler)
					const bool alreadyTransitionedAllSubresources = numSubresourcesTransitioned == numSubresources;
					SEAssert("Transitioned too many subresources", numSubresourcesTransitioned <= numSubresources);

					if (!alreadyTransitionedAllSubresources &&
						pendingStates.HasSubresourceRecord(k_allSubresources))
					{
						const D3D12_RESOURCE_STATES afterState = pendingStates.GetState(k_allSubresources);
						bool insertedTransition = false;
						for (uint32_t subresourceIdx = 0; subresourceIdx < numSubresources; subresourceIdx++)
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
							AddTransitionBarrier(
								resource, globalState.GetState(k_allSubresources), afterState, k_allSubresources, barriers);
						}
						globalResourceStates.SetResourceState(resource, afterState, k_allSubresources, nextFenceVal);
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
							resource, knownStates.GetState(k_allSubresources), k_allSubresources, nextFenceVal);
					}

					for (auto const& knownState : currentknown.second.GetStates())
					{
						const uint32_t subresourceIdx = knownState.first;
						if (subresourceIdx == k_allSubresources)
						{
							continue; // We handled the ALL state fisrt
						}

						globalResourceStates.SetResourceState(
							resource, knownStates.GetState(subresourceIdx), subresourceIdx, nextFenceVal);
					}
				}

				// Add the transition barriers to a command list, if we actually made any:
				if (!barriers.empty())
				{
					std::shared_ptr<dx12::CommandList> barrierCommandList = GetCreateCommandList();

					barrierCommandList->GetD3DCommandList()->ResourceBarrier(
						static_cast<uint32_t>(barriers.size()),
						barriers.data());

					finalCommandLists.emplace_back(barrierCommandList);
				}

				// Add the original command list:
				finalCommandLists.emplace_back(cmdLists[cmdListIdx]);
			}

#if defined(DEBUG_RESOURCE_STATES)
			globalResourceStates.DebugPrintResourceStates();
			LOG("--------------end--------------");
#endif
		} // End barrierLock


		// Insert a GPU wait for any incomplete fences for resources modified on other queues:
		for (uint8_t queueIdx = 0; queueIdx < dx12::CommandListType::CommandListType_Count; queueIdx++)
		{
			const uint64_t currentModificationFence = maxModificationFences[queueIdx];
			if (dx12::Fence::GetRawFenceValue(currentModificationFence) > 0)
			{
				const dx12::CommandListType cmdListType = static_cast<dx12::CommandListType>(queueIdx);
				if (cmdListType != m_type) // Don't wait on resources this queue is about to modify
				{
					dx12::CommandQueue& commandQueue = context->GetCommandQueue(cmdListType);
					if (!commandQueue.GetFence().IsFenceComplete(currentModificationFence))
					{
						GPUWait(commandQueue.GetFence(), currentModificationFence);
					}
				}
			}
		}

		return finalCommandLists;
	}


	uint64_t CommandQueue::Execute(uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists)
	{
		// Ensure any resources used on states only other queue types can manage are in the common state before we
		// attempt to use them:
		TransitionIncompatibleResourceStatesToCommon(numCmdLists, cmdLists);

		// Prepend pending resource barrier command lists to the list of command lists we're executing. This function
		// also records GPU waits on any incomplete fences encountered while parsing the global resource states
		std::vector<std::shared_ptr<dx12::CommandList>> const& finalCommandLists =
			PrependBarrierCommandListsAndWaits(numCmdLists, cmdLists);

		const size_t firstNonBarrierCmdListIdx = finalCommandLists.size() - numCmdLists;

		const uint64_t nextFenceVal = GetNextFenceValue();

		// We'll store the highest modification fence values seen for resources accessed by the submitted command lists
		std::array<uint64_t, dx12::CommandListType::CommandListType_Count> maxModificationFences;
		memset(&maxModificationFences, 0, sizeof(uint64_t) * dx12::CommandListType::CommandListType_Count);

		// Perform the actual execution, now that all of the fixups have happened:
		const uint64_t fenceVal = ExecuteInternal(finalCommandLists, firstNonBarrierCmdListIdx);
		SEAssert("Predicted fence value doesn't match the actual fence value", fenceVal == nextFenceVal);


		// Don't let the caller retain access to a command list in our pool
		for (size_t i = 0; i < numCmdLists; i++)
		{
			cmdLists[i] = nullptr;
		}

		return fenceVal;
	}


	uint64_t CommandQueue::ExecuteInternal(
		std::vector<std::shared_ptr<dx12::CommandList>> const& finalCommandLists, size_t firstNonBarrierCmdListIdx)
	{
		// Get our raw command list pointers, and close them before they're executed
		std::vector<ID3D12CommandList*> commandListPtrs;
		commandListPtrs.reserve(finalCommandLists.size());
		for (uint32_t i = 0; i < finalCommandLists.size(); i++)
		{
			finalCommandLists[i]->Close();
			commandListPtrs.emplace_back(finalCommandLists[i]->GetD3DCommandList());

			SEAssert("We currently only support submitting command lists of the same type to a command queue. "
				"TODO: Support this (e.g. allow submitting compute command lists on a direct queue)",
				finalCommandLists[i]->GetCommandListType() == m_type);
		}

		// Execute the command lists:
		m_commandQueue->ExecuteCommandLists(static_cast<uint32_t>(commandListPtrs.size()), commandListPtrs.data());
		const uint64_t fenceVal = GPUSignal();		

		// Return our command list(s) to the pool:
		for (uint32_t i = 0; i < finalCommandLists.size(); i++)
		{
			finalCommandLists[i]->SetReuseFenceValue(fenceVal);
			m_commandListPool.push(finalCommandLists[i]);
		}

		return fenceVal;
	}


	uint64_t CommandQueue::CPUSignal()
	{
		// Updates the fence to the specified value from the CPU side
		m_fence.CPUSignal(++m_fenceValue); // Note: First (raw) value actually signaled == 1
		return m_fenceValue;
	}


	void CommandQueue::CPUWait(uint64_t fenceValue) const
	{
		m_fence.CPUWait(fenceValue); // CPU waits until fence is set to the given value
	}


	void CommandQueue::Flush()
	{
		const uint64_t fenceValueForSignal = GPUSignal();
		m_fence.CPUWait(fenceValueForSignal);
	}


	uint64_t CommandQueue::GPUSignal()
	{
		GPUSignal(++m_fenceValue);
		return m_fenceValue;
	}


	void CommandQueue::GPUSignal(uint64_t fenceValue)
	{
		HRESULT hr = m_commandQueue->Signal(m_fence.GetD3DFence(), fenceValue);
		CheckHResult(hr, "Command queue failed to issue GPU signal");
	}


	void CommandQueue::GPUWait(uint64_t fenceValue) const
	{
		// Queue a GPU wait (GPU waits until the specified fence reaches/exceeds the fenceValue), and returns immediately
		HRESULT hr = m_commandQueue->Wait(m_fence.GetD3DFence(), fenceValue);
		CheckHResult(hr, "Command queue failed to issue GPU wait");
	}


	void CommandQueue::GPUWait(dx12::Fence& fence, uint64_t fenceValue) const
	{
		// Queue a GPU wait (GPU waits until the specified fence reaches/exceeds the fenceValue), and returns immediately
		HRESULT hr = m_commandQueue->Wait(fence.GetD3DFence(), fenceValue);
		CheckHResult(hr, "Command queue failed to issue GPU wait on externally provided fence");
	}
}