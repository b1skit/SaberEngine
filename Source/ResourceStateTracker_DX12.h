// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>


namespace dx12
{
	class ResourceState
	{
	public:
		ResourceState(D3D12_RESOURCE_STATES);
		ResourceState(ResourceState const&) = default;
		ResourceState(ResourceState&&) = default;
		ResourceState& operator=(ResourceState const&) = default;
		ResourceState& operator=(ResourceState&&) = default;
		~ResourceState() = default;
		
		
		D3D12_RESOURCE_STATES GetState() const;
		void SetState();


	private:
		std::map<uint32_t, D3D12_RESOURCE_STATES> m_subresourceState; // Per-subresource states
		D3D12_RESOURCE_STATES m_resourceState; // State of the entire resource, if no subresource states are specified


	private:
		ResourceState() = delete;
	};


	// Tracks global resource state between threads/command queues/command lists
	class GlobalResourceStateTracker
	{
	public:
		GlobalResourceStateTracker() = default;
		GlobalResourceStateTracker(GlobalResourceStateTracker&&) = default;
		GlobalResourceStateTracker& operator=(GlobalResourceStateTracker&&) = default;
		~GlobalResourceStateTracker() = default;

		// Registration/deregistration: No lock/unlock required
		void RegisterResource(ID3D12Resource*, D3D12_RESOURCE_STATES initialState);
		void UnregisterResource(ID3D12Resource*);

		// Syncronization functions: Threads are responsible for locking/releasing this mutex before/after calling the
		// functions below this point:
		std::mutex& GetGlobalStatesMutex();



	private:
		std::unordered_map<ID3D12Resource*, ResourceState> m_globalStates;
		std::mutex m_globalStatesMutex;


	private: // No copying allowed
		GlobalResourceStateTracker(GlobalResourceStateTracker const&) = delete;
		GlobalResourceStateTracker& operator=(GlobalResourceStateTracker const&) = delete;
	};


	// Tracks local resource state within a command list
	class ResourceStateTracker
	{
	public:
		ResourceStateTracker() = default;
		ResourceStateTracker(ResourceStateTracker&&) = default;
		ResourceStateTracker& operator=(ResourceStateTracker&&) = default;
		~ResourceStateTracker() = default;

		void Reset();


	private:
		std::unordered_map<ID3D12Resource*, ResourceState> m_pendingStates;
		std::unordered_map<ID3D12Resource*, ResourceState> m_knownStates;


	private: // No copying allowed
		ResourceStateTracker(ResourceStateTracker const&) = delete;
		ResourceStateTracker& operator=(ResourceStateTracker const&) = delete;
	};
}