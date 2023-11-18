// © 2023 Adam Badke. All rights reserved.
#include "CommandList_DX12.h"
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "Fence_DX12.h"
#include "ResourceStateTracker_DX12.h"


namespace
{
	constexpr bool IsWriteableState(D3D12_RESOURCE_STATES state)
	{
		switch (state)
		{
		case D3D12_RESOURCE_STATE_RENDER_TARGET:
		case D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
		case D3D12_RESOURCE_STATE_DEPTH_WRITE:
		case D3D12_RESOURCE_STATE_STREAM_OUT:
		case D3D12_RESOURCE_STATE_COPY_DEST:
		case D3D12_RESOURCE_STATE_RESOLVE_DEST:
		case D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE:
		case D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE:
		case D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE:
			return true;
		default:
			return false;
		}
	}
}


namespace dx12
{
	/******************************************************************************************************************/
	// GlobalResourceState
	/******************************************************************************************************************/

	constexpr uint64_t k_invalidLastFence = std::numeric_limits<uint64_t>::max();


	GlobalResourceState::GlobalResourceState(
		D3D12_RESOURCE_STATES initialState, uint32_t numSubresources)
		: IResourceState(initialState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
		, m_numSubresources(numSubresources)
		, m_lastFence(k_invalidLastFence) // Not yet used on a command list
		, m_lastModificationFence(k_invalidLastFence)
	{
		SEAssert("Invalid number of subresources", numSubresources > 0);
	}


	void GlobalResourceState::SetState(
		D3D12_RESOURCE_STATES afterState, SubresourceIdx subresourceIdx, uint64_t lastFence)
	{
		const bool hasOnlyOneSubresource = m_numSubresources == 1;
		IResourceState::SetState(afterState, subresourceIdx, false, hasOnlyOneSubresource);
		
		m_lastFence = lastFence;

		if (IsWriteableState(afterState))
		{
			m_lastModificationFence = lastFence;
		}
	}


	uint32_t GlobalResourceState::GetNumSubresources() const
	{
		return m_numSubresources;
	}


	dx12::CommandListType GlobalResourceState::GetLastCommandListType() const
	{
		if (m_lastFence == k_invalidLastFence)
		{
			return dx12::CommandListType::CommandListType_Invalid;
		}
		return dx12::Fence::GetCommandListTypeFromFenceValue(m_lastFence);
	}


	dx12::CommandListType GlobalResourceState::GetLastModificationCommandListType() const
	{
		if (m_lastModificationFence == k_invalidLastFence)
		{
			return dx12::CommandListType::CommandListType_Invalid;
		}
		return dx12::Fence::GetCommandListTypeFromFenceValue(m_lastModificationFence);
	}


	uint64_t GlobalResourceState::GetLastFenceValue() const
	{
		return m_lastFence;
	}


	uint64_t GlobalResourceState::GetLastModificationFenceValue() const
	{
		SEAssert("If a modification fence has been set, a last fence value must have also been set", 
			m_lastModificationFence == k_invalidLastFence ||
			(m_lastModificationFence != k_invalidLastFence && m_lastFence != k_invalidLastFence));
		
		return m_lastModificationFence;
	}


	/******************************************************************************************************************/
	// LocalResourceState
	/******************************************************************************************************************/


	LocalResourceState::LocalResourceState(D3D12_RESOURCE_STATES initialState, SubresourceIdx subresourceIdx)
		: IResourceState(initialState, subresourceIdx)
	{
	}


	/******************************************************************************************************************/
	// IResourceState
	/******************************************************************************************************************/


	IResourceState::IResourceState(D3D12_RESOURCE_STATES initialState, SubresourceIdx subresourceIdx)
	{
		m_states[subresourceIdx] = initialState;
	}


	IResourceState::~IResourceState()
	{
	}


	D3D12_RESOURCE_STATES IResourceState::GetState(SubresourceIdx subresourceIdx) const
	{
		if (!HasSubresourceRecord(subresourceIdx))
		{
			SEAssert("ResourceState not recorded for the given subresource index, or for all subresources", 
				HasSubresourceRecord(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES));

			return m_states.at(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
		return m_states.at(subresourceIdx);
	}


	void IResourceState::SetState(
		D3D12_RESOURCE_STATES state, SubresourceIdx subresourceIdx, bool isPendingState, bool hasOnlyOneSubresource)
	{
		// Force the global state to always track numeric subresources if only a single subresource exists
		if (hasOnlyOneSubresource)
		{
			SEAssert("The hasOnlyOneSubresource flag is not valid for pending/local resource states", !isPendingState);
			subresourceIdx = 0;
		}

		if ((subresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !isPendingState))
		{
			m_states.clear(); // We don't clear pending transitions: We need to keep any earlier subresource states
		}
		m_states[subresourceIdx] = state;
	}


	void IResourceState::DebugPrintResourceStates() const
	{
		std::string stateStr;
		for (auto const& state : m_states)
		{
			const std::string prefix = state.first >= 1 ? "\tSubresource " : "Subresource ";
			stateStr += prefix + 
				(state.first == 4294967295 ? "ALL" : ("#" + std::to_string(state.first))) +
				": " + 
				GetResourceStateAsCStr(state.second) + "\n";
		}
		LOG(stateStr.c_str());
	}


	/******************************************************************************************************************/
	// GlobalResourceStateTracker
	/******************************************************************************************************************/


	GlobalResourceStateTracker::GlobalResourceStateTracker()
	{
		SEAssert("Invalid fence value cannot map to a valid command list type", 
			dx12::Fence::GetCommandListTypeFromFenceValue(k_invalidLastFence) == dx12::CommandListType::CommandListType_Invalid);
	}


	void GlobalResourceStateTracker::RegisterResource(
		ID3D12Resource* newResource, D3D12_RESOURCE_STATES initialState, uint32_t numSubresources)
	{
		SEAssert("Resource cannot be null", newResource);
		SEAssert("Resource is already registered", !m_globalStates.contains(newResource));
		SEAssert("Invalid number of subresources", numSubresources > 0);

		std::lock_guard<std::mutex> lock(m_globalStatesMutex);
		m_globalStates.emplace(newResource, 
			dx12::GlobalResourceState(initialState, numSubresources));
	}


	void GlobalResourceStateTracker::UnregisterResource(ID3D12Resource* existingResource)
	{
		SEAssert("Resource cannot be null", existingResource);
		SEAssert("Resource is not registered", m_globalStates.contains(existingResource));

		std::lock_guard<std::mutex> lock(m_globalStatesMutex);
		m_globalStates.erase(existingResource);
	}


	// Note: Caller must have called GetGlobalStatesMutex() and locked it before using this function
	GlobalResourceState const& GlobalResourceStateTracker::GetResourceState(ID3D12Resource* resource) const
	{
		// TODO: It's risky to return this value by reference: We should assert the calling thread holds the m_globalStatesMutex
			
		SEAssert("Resource not found, was it registered?", m_globalStates.contains(resource));
		return m_globalStates.at(resource);
	}


	// Note: Caller must have called GetGlobalStatesMutex() and locked it before using this function
	void GlobalResourceStateTracker::SetResourceState(
		ID3D12Resource* resource, D3D12_RESOURCE_STATES newState, SubresourceIdx subresourceIdx, uint64_t lastFence)
	{
		// TODO: We should assert the calling thread currently holds the m_globalStatesMutex

		SEAssert("Resource not found, was it registered?", m_globalStates.contains(resource));
		m_globalStates.at(resource).SetState(newState, subresourceIdx, lastFence);
	}


	void GlobalResourceStateTracker::DebugPrintResourceStates() const
	{
		LOG("--------------\n"
			"\tGlobal States:\n"
			"\t(%s resources)\n"
			"\t--------------", 
			m_globalStates.empty() ? "<empty>" : std::to_string(m_globalStates.size()).c_str());
		for (auto const& resource : m_globalStates)
		{
			LOG("Resource \"%s\", has (%d) subresource%s:", 
				GetDebugName(resource.first).c_str(), 
				resource.second.GetNumSubresources(),
				(resource.second.GetNumSubresources() > 1 ? "s" : ""));
			resource.second.DebugPrintResourceStates();
		}
	}


	/******************************************************************************************************************/
	// LocalResourceStateTracker
	/******************************************************************************************************************/


	bool LocalResourceStateTracker::HasSeenSubresourceInState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) const
	{
		if (m_knownStates.contains(resource)) // No need to check m_pendingStates, we insert to m_knownStates together
		{
			std::map<uint32_t, D3D12_RESOURCE_STATES> const& knownLocalStates = m_knownStates.at(resource).GetStates();
			for (auto const& localResourceState : knownLocalStates)
			{
				if (localResourceState.second == state)
				{
					return true;
				}
			}
		}
		return false;
	}


	bool LocalResourceStateTracker::HasResourceState(ID3D12Resource* resource, SubresourceIdx subresourceIdx) const
	{
		return m_knownStates.contains(resource) && 
			(m_knownStates.at(resource).HasSubresourceRecord(subresourceIdx) || 
			m_knownStates.at(resource).HasSubresourceRecord(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES));
	}


	void LocalResourceStateTracker::SetResourceState(
		ID3D12Resource* resource, D3D12_RESOURCE_STATES stateAfter, SubresourceIdx subresourceIdx)
	{
		// New resource/subresource state:
		if (!m_knownStates.contains(resource))
		{
			const LocalResourceState afterState = LocalResourceState(stateAfter, subresourceIdx);
			
			auto const& emplacePendingResult = m_pendingStates.emplace(resource, afterState);
			SEAssert("Emplace failed. Does the object already exist?", emplacePendingResult.second == true);
			
			auto const& emplaceKnownResult = m_knownStates.emplace(resource, afterState);
			SEAssert("Emplace failed. Does the object already exist?", emplaceKnownResult.second == true);
		}
		else // Existing resource:
		{
			SEAssert("Pending states tracker should contain this resource", m_pendingStates.contains(resource));

			// If we've never seen the subresource, we need to store this transition in the pending list
			if (m_pendingStates.at(resource).HasSubresourceRecord(subresourceIdx) == false)
			{
				m_pendingStates.at(resource).SetState(stateAfter, subresourceIdx, true, false);

				// Note: There is an edge case here where we could set every single subresource index, then set an "ALL"
				// state and it would be (incorrectly) added to the pending list. This is handled during the fixup stage.
			}
			m_knownStates.at(resource).SetState(stateAfter, subresourceIdx, false, false);
		}
	}


	D3D12_RESOURCE_STATES LocalResourceStateTracker::GetResourceState(
		ID3D12Resource* resource, SubresourceIdx subresourceIdx) const
	{
		SEAssert("Trying to get the state of a resource that has not been seen before", 
			m_knownStates.contains(resource));
		return m_knownStates.at(resource).GetState(subresourceIdx);
	}


	void LocalResourceStateTracker::Reset()
	{
		m_pendingStates.clear();
		m_knownStates.clear();
	}


	void LocalResourceStateTracker::DebugPrintResourceStates() const
	{
		LOG("-----------------------\n"
			"\tFinal known states (%s):\n"
			"\t-----------------------",
			m_knownStates.empty() ? "<empty>" : std::to_string(m_knownStates.size()).c_str());
		for (auto const& resource : m_knownStates)
		{
			LOG("Resource \"%s\":", GetDebugName(resource.first).c_str());
			resource.second.DebugPrintResourceStates();
		}
		LOG("------------------------\n"
			"\tPending transitions (%s):\n"
			"\t------------------------", 
			m_pendingStates.empty() ? "<empty>" : std::to_string(m_pendingStates.size()).c_str());
		for (auto const& resource : m_pendingStates)
		{
			LOG("Resource \"%s\":", GetDebugName(resource.first).c_str());
			resource.second.DebugPrintResourceStates();
		}
	}
}