// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Core\Util\ThreadProtector.h"

#include <d3d12.h>


namespace dx12
{
	enum CommandListType; // CommandList_DX12.h

	typedef uint32_t SubresourceIdx;


	/******************************************************************************************************************/
	// IResourceState
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

		void SetState(D3D12_RESOURCE_STATES, SubresourceIdx, bool isPendingState, bool hasOnlyOneSubresource);

		void DebugPrintResourceStates() const;


	private:
		std::map<SubresourceIdx, D3D12_RESOURCE_STATES> m_states;


	private:
		IResourceState() = delete;
	};


	/******************************************************************************************************************/
	// GlobalResourceState
	/******************************************************************************************************************/


	struct GlobalResourceState final : public virtual IResourceState
	{
	public:
		GlobalResourceState(D3D12_RESOURCE_STATES, uint32_t numSubresources);

		void SetState(D3D12_RESOURCE_STATES, SubresourceIdx, uint64_t lastFence);
		uint32_t GetNumSubresources() const;

		// Returns dx12::CommandListType::CommandListType_Invalid if a resource has not been used yet
		dx12::CommandListType GetLastCommandListType() const; 
		dx12::CommandListType GetLastModificationCommandListType() const;

		// In DX12, COPY states are considered different for 3D/Compute vs Copy queues. Resources can only transition
		// out of a COPY state on the same queue type that was used to enter it
		// https://microsoft.github.io/DirectX-Specs/d3d/CPUEfficiency.html#state-support-by-command-list-type
		// We track the last fence value here (which has the command list type packed into its upper bits) to handle
		// this situation. This also allows us to schedule transitions back the COMMON state on the queue type that last
		// used a resource.
		// NOTE: This is not a modification fence; The resource could have been used for anything. This fence represents
		// the last time a resource transition was recorded for any/all subresources.
		uint64_t GetLastFenceValue() const;

		// This fence value is the last time this resource was changed to a state in which it could be modified.
		// Note: m_lastModificationFence <= m_lastFence
		uint64_t GetLastModificationFenceValue() const;


	private:
		uint32_t m_numSubresources;

		uint64_t m_lastFence; // std::numeric_limits<uint64_t>::max() if not yet used on a command list
		uint64_t m_lastModificationFence; // std::numeric_limits<uint64_t>::max() if not yet used on a command list
	};


	/******************************************************************************************************************/
	// LocalResourceState
	/******************************************************************************************************************/


	struct LocalResourceState final : public virtual IResourceState
	{
	public:
		LocalResourceState(D3D12_RESOURCE_STATES, SubresourceIdx);
	};


	/******************************************************************************************************************/
	// GlobalResourceStateTracker
	/******************************************************************************************************************/


	// Tracks global resource state between threads/command queues/command lists
	class GlobalResourceStateTracker
	{
	public:
		GlobalResourceStateTracker();

		~GlobalResourceStateTracker() = default;
		GlobalResourceStateTracker(GlobalResourceStateTracker&&) = default;
		GlobalResourceStateTracker& operator=(GlobalResourceStateTracker&&) = default;

	public:
		// Registration/deregistration: No external locking/unlocking required
		void RegisterResource(ID3D12Resource*, D3D12_RESOURCE_STATES initialState, uint32_t numSubresources);
		void UnregisterResource(ID3D12Resource*);

	public:
		// Syncronization functions: Threads are responsible for locking/releasing before/after calling the
		// functions below this point:
		void AquireLock() const;
		void ReleaseLock() const;

	public:
		// You must have aquired the lock to use these functions, and release it when you're done:
		GlobalResourceState const& GetResourceState(ID3D12Resource*) const;
		void SetResourceState(ID3D12Resource*, D3D12_RESOURCE_STATES, SubresourceIdx, uint64_t lastFence);

		void DebugPrintResourceStates() const;

	private:
		std::unordered_map<ID3D12Resource*, GlobalResourceState> m_globalStates;
		
		mutable std::mutex m_globalStatesMutex;
		mutable util::ThreadProtector m_threadProtector;


	private: // No copying allowed
		GlobalResourceStateTracker(GlobalResourceStateTracker const&) = delete;
		GlobalResourceStateTracker& operator=(GlobalResourceStateTracker const&) = delete;		
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
		LocalResourceStateTracker(LocalResourceStateTracker&&) = default;
		LocalResourceStateTracker& operator=(LocalResourceStateTracker&&) = default;

		bool HasSeenSubresourceInState(ID3D12Resource*, D3D12_RESOURCE_STATES) const;
		bool HasResourceState(ID3D12Resource*, SubresourceIdx) const;
		D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource*, SubresourceIdx) const;
		void SetResourceState(ID3D12Resource*, D3D12_RESOURCE_STATES, SubresourceIdx);

		void Reset();

		std::unordered_map<ID3D12Resource*, LocalResourceState> const& GetPendingResourceStates() const;
		std::unordered_map<ID3D12Resource*, LocalResourceState> const& GetKnownResourceStates() const;

		void DebugPrintResourceStates() const;


	private:
		std::unordered_map<ID3D12Resource*, LocalResourceState> m_pendingStates;
		std::unordered_map<ID3D12Resource*, LocalResourceState> m_knownStates;


	private: // No copying allowed
		LocalResourceStateTracker(LocalResourceStateTracker const&) = delete;
		LocalResourceStateTracker& operator=(LocalResourceStateTracker const&) = delete;		
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

	inline void GlobalResourceStateTracker::AquireLock() const
	{
		m_globalStatesMutex.lock();
		m_threadProtector.TakeOwnership();
	}


	inline void GlobalResourceStateTracker::ReleaseLock() const
	{
		m_threadProtector.ReleaseOwnership();
		m_globalStatesMutex.unlock();
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