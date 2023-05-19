// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>


namespace dx12
{
	class ResourceState
	{
	public:
		ResourceState(ResourceState const&) = default;
		ResourceState(ResourceState&&) = default;
		ResourceState& operator=(ResourceState const&) = default;
		ResourceState& operator=(ResourceState&&) = default;
		
	public:
		ResourceState(D3D12_RESOURCE_STATES, uint32_t subresourceIdx);
		virtual ~ResourceState() = 0;
		
		bool HasSubresourceRecord(uint32_t subresourceIdx) const;

		D3D12_RESOURCE_STATES GetState(uint32_t subresourceIdx) const;
		std::map<uint32_t, D3D12_RESOURCE_STATES> const& GetStates() const;

		void SetState(D3D12_RESOURCE_STATES, uint32_t subresourceIdx, bool isPendingState);


	private:
		std::map<uint32_t, D3D12_RESOURCE_STATES> m_states;

	private:
		ResourceState() = delete;
	};


	struct GlobalResourceState final : public ResourceState
	{
	public:
		GlobalResourceState(D3D12_RESOURCE_STATES, uint32_t subresourceIdx, uint32_t numSubresources);

		void SetState(D3D12_RESOURCE_STATES, uint32_t subresourceIdx);
		uint32_t GetNumSubresources() const;


	private:
		uint32_t m_numSubresources;
	};


	struct LocalResourceState final : public ResourceState
	{
	public:
		LocalResourceState(D3D12_RESOURCE_STATES, uint32_t subresourceIdx);
	};

	

	// Tracks global resource state between threads/command queues/command lists
	class GlobalResourceStateTracker
	{
	public:
		GlobalResourceStateTracker() = default;
		GlobalResourceStateTracker(GlobalResourceStateTracker&&) = default;
		GlobalResourceStateTracker& operator=(GlobalResourceStateTracker&&) = default;
		~GlobalResourceStateTracker() = default;

		// Registration/deregistration: No external locking/unlocking required
		void RegisterResource(ID3D12Resource*, D3D12_RESOURCE_STATES initialState, uint32_t numSubresources);
		void UnregisterResource(ID3D12Resource*);

		// Syncronization functions: Threads are responsible for locking/releasing this mutex before/after calling the
		// functions below this point:
		std::mutex& GetGlobalStatesMutex();

		GlobalResourceState const& GetResourceState(ID3D12Resource*) const;
		void SetResourceState(ID3D12Resource*, D3D12_RESOURCE_STATES, uint32_t subresourceIdx);


	private:
		std::unordered_map<ID3D12Resource*, GlobalResourceState> m_globalStates;
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

		std::unordered_map<ID3D12Resource*, LocalResourceState> const& GetPendingResourceStates() const;


	private:
		std::unordered_map<ID3D12Resource*, LocalResourceState> m_pendingStates;
		std::unordered_map<ID3D12Resource*, LocalResourceState> m_knownStates;


	private: // No copying allowed
		LocalResourceStateTracker(LocalResourceStateTracker const&) = delete;
		LocalResourceStateTracker& operator=(LocalResourceStateTracker const&) = delete;
	};


	// ResourceState
	/******************************************************************************************************************/

	inline bool ResourceState::HasSubresourceRecord(uint32_t subresourceIdx) const
	{
		return m_states.contains(subresourceIdx);
	}


	inline std::map<uint32_t, D3D12_RESOURCE_STATES> const& ResourceState::GetStates() const
	{
		return m_states;
	}


	// GlobalResourceStateTracker
	/******************************************************************************************************************/

	inline std::mutex& GlobalResourceStateTracker::GetGlobalStatesMutex()
	{
		return m_globalStatesMutex;
	}


	// LocalResourceStateTracker
	/******************************************************************************************************************/

	inline std::unordered_map<ID3D12Resource*, LocalResourceState> const& LocalResourceStateTracker::GetPendingResourceStates() const
	{
		return m_pendingStates;
	}
}