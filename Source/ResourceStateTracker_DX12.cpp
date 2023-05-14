// © 2023 Adam Badke. All rights reserved.
#include "DebugConfiguration.h"
#include "ResourceStateTracker_DX12.h"


namespace dx12
{
	// ResourceState
	/******************************************************************************************************************/
	ResourceState::ResourceState(D3D12_RESOURCE_STATES initialState)
	{
		m_resourceState = initialState;
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

		m_globalStates.emplace(newResource, dx12::ResourceState(initialState));
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


	// ResourceStateTracker
	/******************************************************************************************************************/

}