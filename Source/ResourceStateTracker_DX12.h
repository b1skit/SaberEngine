// © 2023 Adam Badke. All rights reserved.
#pragma once
#include <d3d12.h>


namespace dx12
{
	typedef uint32_t SubresourceIdx;


	/******************************************************************************************************************/
	// Resource States
	/******************************************************************************************************************/


	class IResourceState
	{
	public:
		IResourceState(IResourceState const&) = default;
		IResourceState(IResourceState&&) = default;
		IResourceState& operator=(IResourceState const&) = default;
		IResourceState& operator=(IResourceState&&) = default;
				
		
	public:
		IResourceState(D3D12_RESOURCE_STATES, SubresourceIdx);
		virtual ~IResourceState() = 0;
		
		bool HasSubresourceRecord(SubresourceIdx) const;

		D3D12_RESOURCE_STATES GetState(SubresourceIdx) const;
		std::map<uint32_t, D3D12_RESOURCE_STATES> const& GetStates() const;

		void SetState(D3D12_RESOURCE_STATES, SubresourceIdx, bool isPendingState);

		void DebugPrintResourceStates() const;


	private:
		std::map<SubresourceIdx, D3D12_RESOURCE_STATES> m_states;


	private:
		IResourceState() = delete;
	};


	struct GlobalResourceState final : public virtual IResourceState
	{
	public:
		GlobalResourceState(D3D12_RESOURCE_STATES, uint32_t numSubresources);

		void SetState(D3D12_RESOURCE_STATES, SubresourceIdx subresourceIdx);
		uint32_t GetNumSubresources() const;


	private:
		uint32_t m_numSubresources;
	};


	struct LocalResourceState final : public virtual IResourceState
	{
	public:
		LocalResourceState(D3D12_RESOURCE_STATES, uint32_t subresourceIdx);
	};


	/******************************************************************************************************************/
	// GlobalResourceStateTracker
	/******************************************************************************************************************/


	// Tracks global resource state between threads/command queues/command lists
	class GlobalResourceStateTracker
	{
	public:
		GlobalResourceStateTracker() = default;
		~GlobalResourceStateTracker() = default;

		// Registration/deregistration: No external locking/unlocking required
		void RegisterResource(ID3D12Resource*, D3D12_RESOURCE_STATES initialState, uint32_t numSubresources);
		void UnregisterResource(ID3D12Resource*);

		// Syncronization functions: Threads are responsible for locking/releasing this mutex before/after calling the
		// functions below this point:
		std::mutex& GetGlobalStatesMutex();

		GlobalResourceState const& GetResourceState(ID3D12Resource*) const;
		void SetResourceState(ID3D12Resource*, D3D12_RESOURCE_STATES, SubresourceIdx subresourceIdx);

		void DebugPrintResourceStates() const;

	private:
		std::unordered_map<ID3D12Resource*, GlobalResourceState> m_globalStates;
		std::mutex m_globalStatesMutex;


	private: // No copying allowed
		GlobalResourceStateTracker(GlobalResourceStateTracker const&) = delete;
		GlobalResourceStateTracker(GlobalResourceStateTracker&&) = delete;
		GlobalResourceStateTracker& operator=(GlobalResourceStateTracker const&) = delete;
		GlobalResourceStateTracker& operator=(GlobalResourceStateTracker&&) = delete;
	};


	/******************************************************************************************************************/
	// LocalResourceStateTracker
	/******************************************************************************************************************/

	// Tracks local resource state within a command list
	class LocalResourceStateTracker
	{
	public:
		LocalResourceStateTracker() = default;
		~LocalResourceStateTracker() = default;

		bool HasResourceState(ID3D12Resource*, uint32_t subresourceIdx) const;
		D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource*, SubresourceIdx subresourceIdx) const;
		void SetResourceState(ID3D12Resource*, D3D12_RESOURCE_STATES, SubresourceIdx subresourceIdx);

		void Reset();

		std::unordered_map<ID3D12Resource*, LocalResourceState> const& GetPendingResourceStates() const;
		std::unordered_map<ID3D12Resource*, LocalResourceState> const& GetKnownResourceStates() const;

		void DebugPrintResourceStates() const;


	private:
		std::unordered_map<ID3D12Resource*, LocalResourceState> m_pendingStates;
		std::unordered_map<ID3D12Resource*, LocalResourceState> m_knownStates;


	private: // No copying allowed
		LocalResourceStateTracker(LocalResourceStateTracker const&) = delete;
		LocalResourceStateTracker(LocalResourceStateTracker&&) = delete;		
		LocalResourceStateTracker& operator=(LocalResourceStateTracker const&) = delete;
		LocalResourceStateTracker& operator=(LocalResourceStateTracker&&) = delete;
	};


	/******************************************************************************************************************/
	// Resource States
	/******************************************************************************************************************/

	inline bool IResourceState::HasSubresourceRecord(SubresourceIdx subresourceIdx) const
	{
		return m_states.contains(subresourceIdx);
	}


	inline std::map<uint32_t, D3D12_RESOURCE_STATES> const& IResourceState::GetStates() const
	{
		return m_states;
	}


	/******************************************************************************************************************/
	// GlobalResourceStateTracker
	/******************************************************************************************************************/

	inline std::mutex& GlobalResourceStateTracker::GetGlobalStatesMutex()
	{
		return m_globalStatesMutex;
	}


	/******************************************************************************************************************/
	// LocalResourceStateTracker
	/******************************************************************************************************************/

	inline std::unordered_map<ID3D12Resource*, LocalResourceState> const& LocalResourceStateTracker::GetPendingResourceStates() const
	{
		return m_pendingStates;
	}


	inline std::unordered_map<ID3D12Resource*, LocalResourceState> const& LocalResourceStateTracker::GetKnownResourceStates() const
	{
		return m_knownStates;
	}
}