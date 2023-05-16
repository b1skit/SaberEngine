// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>


namespace dx12
{
	class ResourceState
	{
	public:
		ResourceState(D3D12_RESOURCE_STATES, uint32_t subresourceIdx);

		ResourceState(ResourceState const&) = default;
		ResourceState(ResourceState&&) = default;
		ResourceState& operator=(ResourceState const&) = default;
		ResourceState& operator=(ResourceState&&) = default;
		~ResourceState() = default;
		
		bool HasState(uint32_t subresourceIdx) const;

		D3D12_RESOURCE_STATES GetState(uint32_t subresourceIdx) const;
		std::map<uint32_t, D3D12_RESOURCE_STATES> const& GetStates() const;

		void SetState(D3D12_RESOURCE_STATES, uint32_t subresourceIdx);


	private:
		std::map<uint32_t, D3D12_RESOURCE_STATES> m_states;

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

		ResourceState const& GetResourceState(ID3D12Resource*) const;
		void SetResourceState(ID3D12Resource*, D3D12_RESOURCE_STATES, uint32_t subresourceIdx);

		bool ResourceStateMatches(ID3D12Resource*, D3D12_RESOURCE_STATES, uint32_t subresourceIdx) const;

	private:
		std::unordered_map<ID3D12Resource*, ResourceState> m_globalStates;
		std::mutex m_globalStatesMutex;


	private: // No copying allowed
		GlobalResourceStateTracker(GlobalResourceStateTracker const&) = delete;
		GlobalResourceStateTracker& operator=(GlobalResourceStateTracker const&) = delete;
	};


	// Tracks local resource state within a command list
	class LocalResourceStateTracker
	{
	public:
		LocalResourceStateTracker() = default;
		LocalResourceStateTracker(LocalResourceStateTracker&&) = default;
		LocalResourceStateTracker& operator=(LocalResourceStateTracker&&) = default;
		~LocalResourceStateTracker() = default;

		bool HasResourceState(ID3D12Resource*, uint32_t subresourceIdx) const;
		D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource*, uint32_t subresourceIdx) const;
		void SetResourceState(ID3D12Resource*, D3D12_RESOURCE_STATES, uint32_t subresourceIdx);

		void Reset();

		std::unordered_map<ID3D12Resource*, ResourceState> const& GetPendingResourceStates() const;


	private:
		std::unordered_map<ID3D12Resource*, ResourceState> m_pendingStates;
		std::unordered_map<ID3D12Resource*, ResourceState> m_knownStates;


	private: // No copying allowed
		LocalResourceStateTracker(LocalResourceStateTracker const&) = delete;
		LocalResourceStateTracker& operator=(LocalResourceStateTracker const&) = delete;
	};
}