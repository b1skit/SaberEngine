// © 2023 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "ResourceStateTracker_DX12.h"


namespace dx12
{
	// ResourceState
	/******************************************************************************************************************/

	GlobalResourceState::GlobalResourceState(
		D3D12_RESOURCE_STATES initialState, uint32_t subresourceIdx, uint32_t numSubresources)
		: ResourceState(initialState, subresourceIdx)
		, m_numSubresources(numSubresources)
	{
		SEAssert("Invalid number of subresources", numSubresources > 0);
	}


	void GlobalResourceState::SetState(D3D12_RESOURCE_STATES afterState, uint32_t subresourceIdx)
	{
		ResourceState::SetState(afterState, subresourceIdx, false);
	}

	uint32_t GlobalResourceState::GetNumSubresources() const
	{
		return m_numSubresources;
	}


	LocalResourceState::LocalResourceState(D3D12_RESOURCE_STATES initialState, uint32_t subresourceIdx)
		: ResourceState(initialState, subresourceIdx)
	{
	}


	ResourceState::ResourceState(D3D12_RESOURCE_STATES initialState, uint32_t subresourceIdx)
	{
		m_states[subresourceIdx] = initialState;
	}


	ResourceState::~ResourceState()
	{
	}


	D3D12_RESOURCE_STATES ResourceState::GetState(uint32_t subresourceIdx) const
	{
		if (!HasSubresourceRecord(subresourceIdx))
		{
			SEAssert("ResourceState not recorded for the given subresource index, or for all subresources", 
				HasSubresourceRecord(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES));

			return m_states.at(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
		return m_states.at(subresourceIdx);
	}


	void ResourceState::SetState(D3D12_RESOURCE_STATES state, uint32_t subresourceIdx, bool isPendingState)
	{
		if (subresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES && !isPendingState)
		{
			m_states.clear(); // We don't clear pending transitions: We need to keep any earlier subresource states
		}
		m_states[subresourceIdx] = state;
	}


	// GlobalResourceStateTracker
	/******************************************************************************************************************/


	void GlobalResourceStateTracker::RegisterResource(
		ID3D12Resource* newResource, D3D12_RESOURCE_STATES initialState, uint32_t numSubresources)
	{
		// TEMP HAX!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		if (newResource == nullptr)
		{
			LOG_ERROR("SKIPPING NULL TEXTURE REGISTRATION");
			return;
		}


		SEAssert("Resource cannot be null", newResource);
		SEAssert("Resource is already registered", !m_globalStates.contains(newResource));
		SEAssert("Invalid number of subresources", numSubresources > 0);

		std::lock_guard<std::mutex> lock(m_globalStatesMutex);
		m_globalStates.emplace(newResource, 
			dx12::GlobalResourceState(initialState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, numSubresources));
	}


	void GlobalResourceStateTracker::UnregisterResource(ID3D12Resource* existingResource)
	{
		// TEMP HAX!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		if (existingResource == nullptr)
		{
			LOG_ERROR("SKIPPING NULL TEXTURE UNREGISTRATION");
			return;
		}


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
		ID3D12Resource* resource, D3D12_RESOURCE_STATES newState, uint32_t subresourceIdx)
	{
		SEAssert("Resource not found, was it registered?", m_globalStates.contains(resource));
		m_globalStates.at(resource).SetState(newState, subresourceIdx);
	}


	bool LocalResourceStateTracker::HasResourceState(ID3D12Resource* resource, uint32_t subresourceIdx) const
	{
		return m_knownStates.contains(resource) && m_knownStates.at(resource).HasSubresourceRecord(subresourceIdx);
	}


	void LocalResourceStateTracker::SetResourceState(
		ID3D12Resource* resource, D3D12_RESOURCE_STATES stateAfter, uint32_t subresourceIdx)
	{
		if (!m_knownStates.contains(resource)) // New resource:
		{
			const LocalResourceState afterState = LocalResourceState(stateAfter, subresourceIdx);
			m_pendingStates.emplace(resource, afterState);
			m_knownStates.emplace(resource, afterState);
		}
		else // Existing resource:
		{
			SEAssert("Pending states tracker should contain this resource", m_pendingStates.contains(resource));

			// If we've never seen the subresource, we need to store this transition in the pending list
			if (m_pendingStates.at(resource).HasSubresourceRecord(subresourceIdx) == false)
			{
				m_pendingStates.at(resource).SetState(stateAfter, subresourceIdx, true);
			}
			m_knownStates.at(resource).SetState(stateAfter, subresourceIdx, false);
		}
	}


	D3D12_RESOURCE_STATES LocalResourceStateTracker::GetResourceState(
		ID3D12Resource* resource, uint32_t subresourceIdx) const
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
}