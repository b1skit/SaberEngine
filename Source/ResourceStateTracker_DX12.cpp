// © 2023 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "Debug_DX12.h"
#include "ResourceStateTracker_DX12.h"


namespace dx12
{
	/******************************************************************************************************************/
	// Resource States
	/******************************************************************************************************************/

	GlobalResourceState::GlobalResourceState(
		D3D12_RESOURCE_STATES initialState, uint32_t numSubresources)
		: IResourceState(initialState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
		, m_numSubresources(numSubresources)
	{
		SEAssert("Invalid number of subresources", numSubresources > 0);
	}


	void GlobalResourceState::SetState(D3D12_RESOURCE_STATES afterState, SubresourceIdx subresourceIdx)
	{
		IResourceState::SetState(afterState, subresourceIdx, false);
	}

	uint32_t GlobalResourceState::GetNumSubresources() const
	{
		return m_numSubresources;
	}


	LocalResourceState::LocalResourceState(D3D12_RESOURCE_STATES initialState, uint32_t subresourceIdx)
		: IResourceState(initialState, subresourceIdx)
	{
	}


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


	void IResourceState::SetState(D3D12_RESOURCE_STATES state, SubresourceIdx subresourceIdx, bool isPendingState)
	{
		if (subresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !isPendingState)
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
			const std::string prefix = state.first > 1 ? "\tSubresource " : "Subresource ";
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


	GlobalResourceState const& GlobalResourceStateTracker::GetResourceState(ID3D12Resource* resource) const
	{
		SEAssert("Resource not found, was it registered?", m_globalStates.contains(resource));
		return m_globalStates.at(resource);
	}


	void GlobalResourceStateTracker::SetResourceState(
		ID3D12Resource* resource, D3D12_RESOURCE_STATES newState, SubresourceIdx subresourceIdx)
	{
		SEAssert("Resource not found, was it registered?", m_globalStates.contains(resource));
		m_globalStates.at(resource).SetState(newState, subresourceIdx);
	}


	void GlobalResourceStateTracker::DebugPrintResourceStates() const
	{
		LOG("\n---Global States (%s resources)---", m_globalStates.empty() ? "<empty>" : std::to_string(m_globalStates.size()).c_str());
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


	bool LocalResourceStateTracker::HasResourceState(ID3D12Resource* resource, uint32_t subresourceIdx) const
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
				m_pendingStates.at(resource).SetState(stateAfter, subresourceIdx, true);

				// Note: There is an edge case here where we could set every single subresource index, then set an "ALL"
				// state and it would be (incorrectly) added to the pending list. This is handled during the fixup stage.
			}
			m_knownStates.at(resource).SetState(stateAfter, subresourceIdx, false);
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
		LOG("\n---Pending transitions (%s)---", m_pendingStates.empty() ? "<empty>" : std::to_string(m_pendingStates.size()).c_str());
		for (auto const& resource : m_pendingStates)
		{
			LOG("Resource \"%s\":", GetDebugName(resource.first).c_str());
			resource.second.DebugPrintResourceStates();
		}
		LOG("---Known states (%s)---", m_knownStates.empty() ? "<empty>" : std::to_string(m_knownStates.size()).c_str());
		for (auto const& resource : m_knownStates)
		{
			LOG("Resource \"%s\":", GetDebugName(resource.first).c_str());
			resource.second.DebugPrintResourceStates();
		}
	}
}