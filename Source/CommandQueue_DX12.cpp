// © 2022 Adam Badke. All rights reserved.
#include <directx\d3dx12.h> // Must be included BEFORE d3d12.h

#include "Context_DX12.h"
#include "CommandList_DX12.h"
#include "CommandQueue_DX12.h"
#include "Debug_DX12.h"
#include "ProfilingMarkers.h"
#include "SysInfo_DX12.h"
#include "TextUtils.h"

using Microsoft::WRL::ComPtr;


//#define CHECK_TRANSITION_BARRIER_COMMAND_LIST_COMPATIBILITY
//#define DEBUG_FENCES


namespace
{
#if defined(DEBUG_CMD_QUEUE_RESOURCE_TRANSITIONS)
	void DebugPrintBarrier(
		ID3D12Resource* resource, 
		D3D12_RESOURCE_STATES beforeState,
		D3D12_RESOURCE_STATES afterState, 
		uint32_t subresourceIdx)
	{
		std::string const& resourceName = dx12::GetDebugName(resource);

		// Cut down on log spam by filtering output containing keyword substrings
		if (ShouldSkipDebugOutput(resourceName.c_str()))
		{
			return;
		}

		LOG_WARNING("BARRIER: Resource \"%s\"\n\tSubresource #%s: From: %s To: %s",
			resourceName.c_str(),
			(subresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES ? "ALL" : std::to_string(subresourceIdx).c_str()),
			dx12::GetResourceStateAsCStr(beforeState),
			dx12::GetResourceStateAsCStr(afterState));
	}
#endif


	constexpr bool CommandListTypeSupportsState(dx12::CommandListType cmdListType, D3D12_RESOURCE_STATES state)
	{
		if (state == D3D12_RESOURCE_STATE_COMMON)
		{
			return true;
		}

		switch (cmdListType)
		{
		case dx12::CommandListType::Direct:
		{
			constexpr uint32_t k_allSupportedDirectStates = 
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDEX_BUFFER |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_READ |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_STREAM_OUT |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RESOLVE_DEST |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RESOLVE_SOURCE |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

			return state & k_allSupportedDirectStates;
		}
		break;
		case dx12::CommandListType::Compute:
		{
			// Note: We need to explicitely check each state here, as the logical OR will of our supported compute
			// states will match with D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE (as it's an OR with 
			// D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE

			return state == D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON ||
				state == D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS ||
				state == D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		}
		break;
		case dx12::CommandListType::Copy:
		{
			// Note: The copy queue only supports the COPY_SOURCE and COPY_DEST states, but they're considered different to
			// the COPY_SOURCE/COPY_DEST states on direct and compute queues
			constexpr uint32_t k_allSupportedCopyStates = 
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST |
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE;

			return state & k_allSupportedCopyStates;
		}
		break;
		case dx12::CommandListType::Bundle:
		case dx12::CommandListType::VideoDecode:
		case dx12::CommandListType::VideoProcess:
		case dx12::CommandListType::VideoEncode:
		default: SEAssertF("Invalid/currently unsupported command list type");
		}
		return false; // This should never happen
	}


	bool NeedsCommonTransition(
		D3D12_RESOURCE_STATES currentGlobalState, 
		dx12::CommandListType srcCmdListType, 
		dx12::CommandListType dstCmdListType)
	{
		// This function is based on the information on these pages:
		// https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html#command-queue-layout-compatibility
		// https://microsoft.github.io/DirectX-Specs/d3d/CPUEfficiency.html#state-support-by-command-list-type

		SEAssert(currentGlobalState != D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			"Invalid state for transition");

		SEAssert(currentGlobalState != D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ,
			"We should genenerally avoid this state. See this page for more info: "
			"https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states");

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
		case dx12::CommandListType::Compute:
		{
			const bool dstCmdListSupportsCurrentState = CommandListTypeSupportsState(dstCmdListType, currentGlobalState);
			return dstCmdListSupportsCurrentState == false;
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
		dx12::CommandListType cmdListType,
		ID3D12Resource* resource,
		D3D12_RESOURCE_STATES beforeState,
		D3D12_RESOURCE_STATES afterState,
		uint32_t subresourceIdx,
		std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAGS::D3D12_RESOURCE_BARRIER_FLAG_NONE)
	{
#if defined(CHECK_TRANSITION_BARRIER_COMMAND_LIST_COMPATIBILITY)
		SEAssert("Attempting to record an transition type not supported by the command list type", 
			CommandListTypeSupportsState(cmdListType, beforeState) &&
			CommandListTypeSupportsState(cmdListType, afterState));
#endif

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
#if defined(DEBUG_CMD_QUEUE_RESOURCE_TRANSITIONS)
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

		const D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {
			.Type = m_d3dType,
			.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE, // None, or Disable Timeout
			.NodeMask = dx12::SysInfo::GetDeviceNodeMask()};

		std::string fenceEventName;
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

		const std::wstring cmdQueueName = 
			std::wstring(dx12::CommandList::GetCommandListTypeWName(m_type)) + L"_CommandQueue";
		m_commandQueue->SetName(cmdQueueName.c_str());

		m_fence.Create(m_deviceCache, fenceEventName.c_str());
		m_typeFenceBitMask = dx12::Fence::GetCommandListTypeFenceMaskBits(type);
		m_fenceValue = m_typeFenceBitMask; // Fence value effectively starts at 0, with the type bits set

		SEAssert(dx12::Fence::GetRawFenceValue(m_fenceValue) == 0,
			"The fence value should be 0 after removing the command queue type bits");

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
	// transition any resources in incompatible states back to common on a compatible command queue type.
	// Note: We're recording/submitting command lists to different command queue types here: This should be done 
	// single-threaded, like all other command list submissions
	void CommandQueue::TransitionIncompatibleResourceStatesToCommon(
		uint32_t numCmdLists,
		std::shared_ptr<dx12::CommandList>* cmdLists)
	{
		SEBeginCPUEvent("CommandQueue::TransitionIncompatibleResourceStatesToCommon");

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

		dx12::GlobalResourceStateTracker& globalResourceStates = context->GetGlobalResourceStates();


		auto ConfigureTransitionPtrs = [&](
			dx12::CommandListType lastCmdListType,
			std::vector<D3D12_RESOURCE_BARRIER>*& targetBarriers, 
			uint64_t& nextFenceValue)
		{
			switch (lastCmdListType)
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
		};


		auto AddCommonTransitionAndUpdateGlobalState = [&](
			dx12::CommandListType lastCmdListType, 
			ID3D12Resource* resource, 
			std::vector<D3D12_RESOURCE_BARRIER>* targetBarriers,
			uint64_t& nextFenceValue,
			D3D12_RESOURCE_STATES before, 
			uint32_t subresourceIdx)
		{
			AddTransitionBarrier(
				lastCmdListType,
				resource,
				before,
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
				subresourceIdx,
				*targetBarriers);

			globalResourceStates.SetResourceState(
				resource,
				D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON,
				subresourceIdx,
				nextFenceValue);
		};

		
		{
			std::lock_guard<std::mutex> barrierLock(globalResourceStates.GetGlobalStatesMutex());

#if defined(DEBUG_CMD_QUEUE_RESOURCE_TRANSITIONS)
			LOG_WARNING("\n--------------------- TransitionIncompatibleResourceStatesToCommon() ---------------------\n"
				"\t\"%s\":",
				dx12::GetDebugName(m_commandQueue.Get()).c_str());
#endif

			for (uint32_t cmdListIdx = 0; cmdListIdx < numCmdLists; cmdListIdx++)
			{
				LocalResourceStateTracker const& localResourceTracker = cmdLists[cmdListIdx]->GetLocalResourceStates();

				// Check the *pending* states held by the command list we're about to submit:
				for (auto const& currentPending : localResourceTracker.GetPendingResourceStates())
				{
					ID3D12Resource* currentResource = currentPending.first;
					
					dx12::GlobalResourceState const& globalResourceState = 
						globalResourceStates.GetResourceState(currentResource);
					
					const dx12::CommandListType lastCmdListType = globalResourceState.GetLastCommandListType();
					if (lastCmdListType == dx12::CommandListType::CommandListType_Invalid)
					{
						continue; // Resource not used yet
					}

					// Here, we check for (sub)resources in the pending list that have an incompatible BEFORE state in
					// the global state tracker. If we find any, we transition them to common so the next transitions
					// we make on this queue will be from a state we can handle.
					// After this function completes the pending list is unchanged, but any incompatible global states
					// have been transitioned to COMMON

					const bool hasGlobalAllSubresourcesRecord = 
						globalResourceState.HasSubresourceRecord(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
					const size_t numGlobalResourceStateRecords = globalResourceState.GetStates().size();
					const uint32_t numSubresources = globalResourceState.GetNumSubresources();

					// 1) If we've only got a global ALL subresource record, we just need to handle that
					bool hasRemainingSubresourcesToCheck = true;
					if (hasGlobalAllSubresourcesRecord &&
						numGlobalResourceStateRecords == 1)
					{
						const D3D12_RESOURCE_STATES globalAllSubresourceState = 
							globalResourceState.GetState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

						if (NeedsCommonTransition(globalAllSubresourceState, lastCmdListType, m_type))
						{
							std::vector<D3D12_RESOURCE_BARRIER>* targetBarriers = nullptr;
							uint64_t nextFenceValue = 0;

							ConfigureTransitionPtrs(lastCmdListType, targetBarriers, nextFenceValue);

							AddCommonTransitionAndUpdateGlobalState(
								lastCmdListType,
								currentResource,
								targetBarriers,
								nextFenceValue,
								globalAllSubresourceState,
								D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

							hasRemainingSubresourcesToCheck = false;
						}
					}

					// 2) We have multiple records: We must check the individual subresource records first:
					if (hasRemainingSubresourcesToCheck)
					{
						uint32_t numSubresourcesProcessed = 0;
						std::vector<bool> processedSubresourceIdxs(numSubresources, false);

						for (auto const& globalState : globalResourceState.GetStates())
						{
							// Skip ALL subresource transitions for now, we handle them last:
							const uint32_t globalStateSubresourceIdx = globalState.first;
							if (globalStateSubresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
							{
								continue;
							}

							const D3D12_RESOURCE_STATES globalD3DState = globalState.second;

							// Handle individual subresources that are in an incompatible state for the current queue:
							if (NeedsCommonTransition(globalD3DState, lastCmdListType, m_type))
							{
								// Transition the resource from its current global state to common:
								std::vector<D3D12_RESOURCE_BARRIER>* targetBarriers = nullptr;
								uint64_t nextFenceValue = 0;

								ConfigureTransitionPtrs(lastCmdListType, targetBarriers, nextFenceValue);

								AddCommonTransitionAndUpdateGlobalState(
									lastCmdListType,
									currentResource,
									targetBarriers,
									nextFenceValue,
									globalD3DState,
									globalStateSubresourceIdx);
							}

							// We've checked if this subresource index needs a transition, and handled it if it did
							numSubresourcesProcessed++;
							processedSubresourceIdxs[globalStateSubresourceIdx] = true;
						}
						SEAssert(numSubresourcesProcessed <= numSubresources, "Transitioned too many subresources");

						// Check: Did we transition all subresources already? If so, we can skip the ALL transition
						hasRemainingSubresourcesToCheck = numSubresourcesProcessed < numSubresources;

						// 3) Anything remaining at this point is resolved by the ALL subresources record, and does not
						// have an actual subresource record to represent it.
						if (hasRemainingSubresourcesToCheck)
						{
							SEAssert(globalResourceState.HasSubresourceRecord(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES),
								"We have remaining subresources to check, but the global resource state is missing"
								" an ALL subresource record. This shouldn't be possible");
							
							const D3D12_RESOURCE_STATES globalAllState = 
								globalResourceState.GetState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

							// As any remaining subresources are resolved by the ALL subresources record, we only need
							// to record more transitions if the ALL state is compatible with the current queue
							if (NeedsCommonTransition(globalAllState, lastCmdListType, m_type))
							{
								std::vector<D3D12_RESOURCE_BARRIER>* targetBarriers = nullptr;
								uint64_t nextFenceValue = 0;

								ConfigureTransitionPtrs(lastCmdListType, targetBarriers, nextFenceValue);

								// Handle any remaining individual subresources that have a global state covered by the ALL 
								// subresources record
								for (uint32_t subresourceIdx = 0; subresourceIdx < numSubresources; subresourceIdx++)
								{
									const bool didProcessSubresource = processedSubresourceIdxs[subresourceIdx];
									if (!didProcessSubresource)
									{
										AddCommonTransitionAndUpdateGlobalState(
											lastCmdListType,
											currentResource,
											targetBarriers,
											nextFenceValue,
											globalAllState,
											subresourceIdx);
									}
								}
							}
						}
					}
				}
			}
		} // End barrierLock

		// Execute our transitions to COMMON, and have our main command queue wait on GPU fences to ensure the 
		// transitions are complete before proceeding
		// Note: We're going to submit our COMMON resource transitions on new discrete command lists executed on the
		// same queue a resource was last used on. Thus, we don't need to fence on any previous work in these queues
		if (!directBarriers.empty())
		{
			directCmdList->ResourceBarrier(
				static_cast<uint32_t>(directBarriers.size()),
				directBarriers.data());

			const uint64_t directBarrierFence = directQueue.ExecuteInternal({ directCmdList });

			GPUWait(directQueue.GetFence(), directBarrierFence);
		}
		if (!computeBarriers.empty())
		{
			computeCmdList->ResourceBarrier(
				static_cast<uint32_t>(computeBarriers.size()),
				computeBarriers.data());

			const uint64_t computeBarrierFence = computeQueue.ExecuteInternal({ computeCmdList });

			GPUWait(computeQueue.GetFence(), computeBarrierFence);
		}
		if (!copyBarriers.empty())
		{
			copyCmdList->ResourceBarrier(
				static_cast<uint32_t>(copyBarriers.size()),
				copyBarriers.data());

			const uint64_t copyBarrierFence = copyQueue.ExecuteInternal({ copyCmdList });

			GPUWait(copyQueue.GetFence(), copyBarrierFence);
		}


#if defined(DEBUG_CMD_QUEUE_RESOURCE_TRANSITIONS)
		LOG_WARNING("\n------------ !DONE! TransitionIncompatibleResourceStatesToCommon() !DONE! ------------\n");
#endif

		SEEndCPUEvent();
	}


	std::vector<std::shared_ptr<dx12::CommandList>> CommandQueue::PrependBarrierCommandListsAndWaits(
		uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists)
	{
		SEBeginCPUEvent("CommandQueue::PrependBarrierCommandListsAndWaits");

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

#if defined(DEBUG_STATE_TRACKER_RESOURCE_TRANSITIONS)
			LOG_WARNING("\n--------------------- PrependBarrierCommandListsAndWaits() ---------------------\n"
				"\t\"%s\":",
				dx12::GetDebugName(m_commandQueue.Get()).c_str());
			globalResourceStates.DebugPrintResourceStates();
#endif

			const uint64_t nextFenceVal = GetNextFenceValue();

			for (uint32_t cmdListIdx = 0; cmdListIdx < numCmdLists; cmdListIdx++)
			{
				std::vector<D3D12_RESOURCE_BARRIER> barriers;

				LocalResourceStateTracker const& localResourceTracker = cmdLists[cmdListIdx]->GetLocalResourceStates();

#if defined(DEBUG_STATE_TRACKER_RESOURCE_TRANSITIONS)
				cmdLists[cmdListIdx]->DebugPrintResourceStates();

				LOG_WARNING("\n-------------------------\n"
					"\tPrepended fixup barriers:\n"
					"\t-------------------------");
#endif

				// Handle pending transitions for the current command list:
				for (auto const& currentPending : localResourceTracker.GetPendingResourceStates())
				{
					ID3D12Resource* resource = currentPending.first;
					dx12::LocalResourceState const& pendingStates = currentPending.second;
					GlobalResourceState const& globalState = globalResourceStates.GetResourceState(resource);

					// Cache the global modification value: We'll GPU wait on the most recent modification fence
					const dx12::CommandListType lastModificationCmdListType = 
						globalState.GetLastModificationCommandListType();

					// Has it ever been modified on a command list?
					if (lastModificationCmdListType != dx12::CommandListType::CommandListType_Invalid)
					{
						maxModificationFences[lastModificationCmdListType] = std::max(
							globalState.GetLastModificationFenceValue(),
							maxModificationFences[lastModificationCmdListType]);
					}

					const uint32_t numSubresources = globalState.GetNumSubresources();

					uint32_t numSubresourcesTransitioned = 0;
					for (auto const& pendingState : pendingStates.GetStates())
					{
						const uint32_t subresourceIdx = pendingState.first;
						if (subresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
						{
							continue; // We'll handle the ALL state last
						}

						const D3D12_RESOURCE_STATES beforeState = globalState.GetState(subresourceIdx);
						const D3D12_RESOURCE_STATES afterState = pendingState.second;
						if (beforeState != afterState)
						{
							AddTransitionBarrier(
								cmdLists[cmdListIdx]->GetCommandListType(), 
								resource, 
								beforeState, 
								afterState, 
								subresourceIdx, 
								barriers);
							globalResourceStates.SetResourceState(resource, afterState, subresourceIdx, nextFenceVal);
							numSubresourcesTransitioned++;
						}
					}

					// Note: There is an edge case where we could individually add each subresource to the pending list,
					// and then add an "ALL" transition which would be (incorrectly) added to the pending list. So, we
					// handle that here (as it makes the bookkeeping much simpler)
					const bool alreadyTransitionedAllSubresources = numSubresourcesTransitioned == numSubresources;
					SEAssert(numSubresourcesTransitioned <= numSubresources, "Transitioned too many subresources");

					if (!alreadyTransitionedAllSubresources &&
						pendingStates.HasSubresourceRecord(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES))
					{
						const D3D12_RESOURCE_STATES afterState = 
							pendingStates.GetState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
						bool insertedTransition = false;
						for (uint32_t subresourceIdx = 0; subresourceIdx < numSubresources; subresourceIdx++)
						{
							const D3D12_RESOURCE_STATES beforeState = globalState.GetState(subresourceIdx);
							if (beforeState != afterState)
							{
								AddTransitionBarrier(
									cmdLists[cmdListIdx]->GetCommandListType(), 
									resource, 
									beforeState, 
									afterState, 
									subresourceIdx, 
									barriers);
								insertedTransition = true;
							}
						}
						if (!insertedTransition)
						{
							AddTransitionBarrier(
								cmdLists[cmdListIdx]->GetCommandListType(),
								resource, 
								globalState.GetState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES),
								afterState, 
								D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
								barriers);
						}
						globalResourceStates.SetResourceState(
							resource, afterState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, nextFenceVal);
					}
				}

				// Finally, update the global state from the known final local states:
				for (auto const& currentknown : localResourceTracker.GetKnownResourceStates())
				{
					ID3D12Resource* resource = currentknown.first;
					dx12::LocalResourceState const& knownStates = currentknown.second;
					GlobalResourceState const& globalState = globalResourceStates.GetResourceState(resource);

					// Set the ALL state first:
					if (knownStates.HasSubresourceRecord(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES))
					{
						globalResourceStates.SetResourceState(
							resource, 
							knownStates.GetState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES), 
							D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, 
							nextFenceVal);
					}

					for (auto const& knownState : currentknown.second.GetStates())
					{
						const uint32_t subresourceIdx = knownState.first;
						if (subresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
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

					barrierCommandList->ResourceBarrier(
						static_cast<uint32_t>(barriers.size()),
						barriers.data());

					finalCommandLists.emplace_back(barrierCommandList);

#if defined(DEBUG_CMD_QUEUE_RESOURCE_TRANSITIONS)
					LOG_WARNING("\nRecorded %llu resource transition barriers to fixup command list \"%s\"...\n",
						barriers.size(),
						GetDebugName(barrierCommandList->GetD3DCommandList()).c_str());
#endif
				}

				// Add the original command list:
				finalCommandLists.emplace_back(cmdLists[cmdListIdx]);
			}

#if defined(DEBUG_STATE_TRACKER_RESOURCE_TRANSITIONS)
			globalResourceStates.DebugPrintResourceStates();
			LOG_WARNING("-------------- !DONE! PrependBarrierCommandListsAndWaits() !DONE! --------------");
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

		SEEndCPUEvent();

		return finalCommandLists;
	}


	uint64_t CommandQueue::Execute(uint32_t numCmdLists, std::shared_ptr<dx12::CommandList>* cmdLists)
	{
		SEBeginCPUEvent(std::format("CommandQueue::Execute ({})", CommandList::GetCommandListTypeName(m_type)).c_str());

		// Ensure any resources used with states only other queue types can manage are in the common state before we
		// attempt to use them:
		TransitionIncompatibleResourceStatesToCommon(numCmdLists, cmdLists);

		// Prepend pending resource barrier command lists to the list of command lists we're executing. This function
		// also records GPU waits on any incomplete fences encountered while parsing the global resource states
		std::vector<std::shared_ptr<dx12::CommandList>> const& finalCommandLists =
			PrependBarrierCommandListsAndWaits(numCmdLists, cmdLists);

		// Perform the actual execution, now that all of the fixups have happened:
		const uint64_t fenceVal = ExecuteInternal(finalCommandLists);

		// Don't let the caller retain access to a command list in our pool
		for (size_t i = 0; i < numCmdLists; i++)
		{
			cmdLists[i] = nullptr;
		}

		SEEndCPUEvent();

		return fenceVal;
	}


	uint64_t CommandQueue::ExecuteInternal(std::vector<std::shared_ptr<dx12::CommandList>> const& finalCommandLists)
	{
		SEBeginCPUEvent("CommandQueue::ExecuteInternal");

		// Get our raw command list pointers, and close them before they're executed
		std::vector<ID3D12CommandList*> commandListPtrs;
		commandListPtrs.reserve(finalCommandLists.size());
		for (uint32_t i = 0; i < finalCommandLists.size(); i++)
		{
#if defined(DEBUG_CMD_QUEUE_RESOURCE_TRANSITIONS)
			LOG_WARNING(std::format("Queue \"{}\" executing command list {}/{}: \"{}\"",
				dx12::GetDebugName(m_commandQueue.Get()).c_str(),
				(i + 1),
				finalCommandLists.size(),
				dx12::GetDebugName(finalCommandLists[i]->GetD3DCommandList()).c_str()).c_str());
#endif

			finalCommandLists[i]->Close();
			commandListPtrs.emplace_back(finalCommandLists[i]->GetD3DCommandList());

			SEAssert(finalCommandLists[i]->GetCommandListType() == m_type,
				"We currently only support submitting command lists of the same type to a command queue. "
				"TODO: Support this (e.g. allow submitting compute command lists on a direct queue)");
		}
		
		// Execute the command lists:
		SEBeginGPUEvent(m_commandQueue.Get(), 
			perfmarkers::Type::GraphicsQueue, 
			std::format("{} command queue", dx12::CommandList::GetCommandListTypeName(m_type)).c_str());

//#define SUBMIT_COMMANDLISTS_IN_SERIAL
#if defined(SUBMIT_COMMANDLISTS_IN_SERIAL)
		for (size_t i = 0; i < commandListPtrs.size(); i++)
		{
			m_commandQueue->ExecuteCommandLists(1u, &commandListPtrs[i]);
		}
#else
		m_commandQueue->ExecuteCommandLists(static_cast<uint32_t>(commandListPtrs.size()), commandListPtrs.data());
#endif

		SEEndGPUEvent(m_commandQueue.Get());

		const uint64_t fenceVal = GPUSignal();

//#define DISABLE_FRAME_BUFFERING
#if defined(DISABLE_FRAME_BUFFERING)
		CPUWait(fenceVal);
#endif

		// Return our command list(s) to the pool:
		for (uint32_t i = 0; i < finalCommandLists.size(); i++)
		{
			finalCommandLists[i]->SetReuseFenceValue(fenceVal);
			m_commandListPool.push(finalCommandLists[i]);
		}

		SEEndCPUEvent();

		return fenceVal;
	}


	uint64_t CommandQueue::CPUSignal()
	{
#if defined(DEBUG_FENCES)
		LOG_WARNING("CommandQueue::CPUSignal: %s, %llu = %llu", 
			dx12::GetDebugName(m_commandQueue.Get()).c_str(), 
			m_fenceValue + 1,
			dx12::Fence::GetRawFenceValue(m_fenceValue + 1));
#endif

		// Updates the fence to the specified value from the CPU side
		m_fence.CPUSignal(++m_fenceValue); // Note: First (raw) value actually signaled == 1
		return m_fenceValue;
	}


	void CommandQueue::CPUWait(uint64_t fenceValue) const
	{
		SEAssert(dx12::Fence::GetCommandListTypeFromFenceValue(fenceValue) != dx12::CommandListType::CommandListType_Invalid,
			"Attempting to CPUWait on a fence from an invalid CommandListType");

#if defined(DEBUG_FENCES)
		LOG_WARNING("CommandQueue::CPUWait: %s, %llu = %llu",
			dx12::GetDebugName(m_commandQueue.Get()).c_str(),
			fenceValue,
			dx12::Fence::GetRawFenceValue(fenceValue));
#endif

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
		SEAssert(dx12::Fence::GetCommandListTypeFromFenceValue(fenceValue) != dx12::CommandListType::CommandListType_Invalid,
			"Attempting to GPUSignal with a fence from an invalid CommandListType");

#if defined(DEBUG_FENCES)
		LOG_WARNING("CommandQueue::GPUSignal: %s, %llu = %llu",
			dx12::GetDebugName(m_commandQueue.Get()).c_str(),
			fenceValue,
			dx12::Fence::GetRawFenceValue(fenceValue));
#endif

		HRESULT hr = m_commandQueue->Signal(m_fence.GetD3DFence(), fenceValue);
		CheckHResult(hr, "Command queue failed to issue GPU signal");
	}


	void CommandQueue::GPUWait(uint64_t fenceValue) const
	{
		SEAssert(dx12::Fence::GetCommandListTypeFromFenceValue(fenceValue) != dx12::CommandListType::CommandListType_Invalid,
			"Attempting to GPUWait on a fence from an invalid CommandListType");

#if defined(DEBUG_FENCES)
		LOG_WARNING("CommandQueue::GPUWait: %s, %llu = %llu",
			dx12::GetDebugName(m_commandQueue.Get()).c_str(),
			fenceValue,
			dx12::Fence::GetRawFenceValue(fenceValue));
#endif

		// Queue a GPU wait (GPU waits until the specified fence reaches/exceeds the fenceValue), and returns immediately
		HRESULT hr = m_commandQueue->Wait(m_fence.GetD3DFence(), fenceValue);
		CheckHResult(hr, "Command queue failed to issue GPU wait");
	}


	void CommandQueue::GPUWait(dx12::Fence& fence, uint64_t fenceValue) const
	{
		SEAssert(dx12::Fence::GetCommandListTypeFromFenceValue(fenceValue) != dx12::CommandListType::CommandListType_Invalid,
			"Attempting to GPUWait on a fence from an invalid CommandListType");

#if defined(DEBUG_FENCES)
		LOG_WARNING("CommandQueue::GPUWait on another fence: \"%s\" waiting on \"%s\" from queue type \"%s\" for value %llu = %llu",
			dx12::GetDebugName(m_commandQueue.Get()).c_str(),
			dx12::GetDebugName(fence.GetD3DFence()).c_str(),
			dx12::CommandList::GetCommandListTypeName(dx12::Fence::GetCommandListTypeFromFenceValue(fenceValue)),
			fenceValue,
			dx12::Fence::GetRawFenceValue(fenceValue));
#endif

		// Queue a GPU wait (GPU waits until the specified fence reaches/exceeds the fenceValue), and returns immediately
		HRESULT hr = m_commandQueue->Wait(fence.GetD3DFence(), fenceValue);
		CheckHResult(hr, "Command queue failed to issue GPU wait on externally provided fence");
	}
}