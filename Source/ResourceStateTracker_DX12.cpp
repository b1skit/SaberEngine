// © 2023 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "ResourceStateTracker_DX12.h"


namespace dx12
{
	// ResourceState
	/******************************************************************************************************************/
	ResourceState::ResourceState(D3D12_RESOURCE_STATES initialState, uint32_t subresourceIdx)
	{
		m_states[subresourceIdx] = initialState;
	}


	bool ResourceState::HasState(uint32_t subresourceIdx) const
	{
		return m_states.contains(subresourceIdx);
	}


	D3D12_RESOURCE_STATES ResourceState::GetState(uint32_t subresourceIdx) const
	{
		if (!HasState(subresourceIdx))
		{
			SEAssert("ResourceState not recorded for the given subresource index, or for all subresources", 
				HasState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES));

			return m_states.at(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		}
		return m_states.at(subresourceIdx);
	}
	

	std::map<uint32_t, D3D12_RESOURCE_STATES> const& ResourceState::GetStates() const
	{
		return m_states;
	}


	void ResourceState::SetState(D3D12_RESOURCE_STATES state, uint32_t subresourceIdx)
	{
		if (subresourceIdx == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
		{
			m_states.clear();
		}
		m_states[subresourceIdx] = state;
	}


	// GlobalResourceStateTracker
	/******************************************************************************************************************/


	void GlobalResourceStateTracker::RegisterResource(ID3D12Resource* newResource, D3D12_RESOURCE_STATES initialState)
	{
		// TEMP HAX!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		if (newResource == nullptr)
		{
			LOG_ERROR("SKIPPING NULL TEXTURE REGISTRATION");
			return;
		}


		SEAssert("Resource cannot be null", newResource);
		SEAssert("Resource is already registered", !m_globalStates.contains(newResource));

		m_globalStates.emplace(newResource, dx12::ResourceState(initialState, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES));
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

		m_globalStates.erase(existingResource);
	}


	std::mutex& GlobalResourceStateTracker::GetGlobalStatesMutex()
	{
		return m_globalStatesMutex;
	}


	ResourceState const& GlobalResourceStateTracker::GetResourceState(
		ID3D12Resource* resource) const
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


	bool GlobalResourceStateTracker::ResourceStateMatches(
		ID3D12Resource* resource, D3D12_RESOURCE_STATES state, uint32_t subresourceIdx) const
	{
		SEAssert("Resource not found, was it registered?", m_globalStates.contains(resource));

		auto const& globalState = m_globalStates.find(resource);
		return globalState->second.HasState(subresourceIdx) &&
			globalState->second.GetState(subresourceIdx) == state;
	}


	// LocalResourceStateTracker
	/******************************************************************************************************************/


	bool LocalResourceStateTracker::HasResourceState(ID3D12Resource* resource, uint32_t subresourceIdx) const
	{
		return m_knownStates.contains(resource) && m_knownStates.at(resource).HasState(subresourceIdx);
	}


	void LocalResourceStateTracker::SetResourceState(
		ID3D12Resource* resource, D3D12_RESOURCE_STATES stateAfter, uint32_t subresourceIdx)
	{
		if (!m_knownStates.contains(resource)) // New resource:
		{
			const ResourceState afterState = ResourceState(stateAfter, subresourceIdx);
			m_pendingStates.emplace(resource, afterState);
			m_knownStates.emplace(resource, afterState);
		}
		else // Existing resource:
		{
			m_knownStates.at(resource).SetState(stateAfter, subresourceIdx);
		}
	}


	D3D12_RESOURCE_STATES LocalResourceStateTracker::GetResourceState(
		ID3D12Resource* resource, uint32_t subresourceIdx) const
	{
		SEAssert("Trying to get the state of a resource that has not been seen before", m_knownStates.contains(resource));
		return m_knownStates.at(resource).GetState(subresourceIdx);
	}


	void LocalResourceStateTracker::Reset()
	{
		m_pendingStates.clear();
		m_knownStates.clear();
	}


	std::unordered_map<ID3D12Resource*, ResourceState> const& LocalResourceStateTracker::GetPendingResourceStates() const
	{
		return m_pendingStates;
	}
}