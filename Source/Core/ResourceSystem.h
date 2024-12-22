// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Interfaces/ILoadContext.h"

#include "Util/HashUtils.h"
#include "Util/DataHash.h"


namespace core
{
	enum class ResourceState : uint8_t
	{
		Empty,
		Loading,
		Ready,
		Released,
		Error,
	};


	class IResourceSystem
	{
	public:
		virtual ~IResourceSystem() = default;


	private:
		friend class Inventory;

		virtual void Destroy() = 0;
		virtual void OnEndOfFrame() = 0;
	};


	template<typename T>
	class ResourceSystem final : public virtual IResourceSystem
	{
	public:
		using RefCountType = uint32_t; // Type to use for the reference counter


	public:
// Put our atomics on their own cache lines
#define CACHE_LINE_SIZE 64 
		struct alignas(CACHE_LINE_SIZE) ControlBlock
		{
			std::unique_ptr<T>* m_object = nullptr; // The InvPtr populates our unique_ptr asyncronously

			util::DataHash m_id = 0;
			ResourceSystem<T>* m_owningResourceSystem = nullptr;

			alignas(CACHE_LINE_SIZE) std::atomic<RefCountType> m_refCount = 0;
			alignas(CACHE_LINE_SIZE) std::atomic<ResourceState> m_state = ResourceState::Empty;
		};
#undef CACHE_LINE_SIZE

		struct PtrAndControl
		{
			std::unique_ptr<T> m_object;
			std::unique_ptr<ControlBlock> m_control;
			bool m_isPermanent = false;
		};
		

	public:
		ResourceSystem();

		ResourceSystem(ResourceSystem const&) = default;
		ResourceSystem(ResourceSystem&&) = default;

		ResourceSystem& operator=(ResourceSystem const&) = default;
		ResourceSystem& operator=(ResourceSystem&&) = default;

		~ResourceSystem() = default;

		void Destroy() override;


	public:
		bool HasLoaded(util::DataHash) const;

		template<typename L>
		ControlBlock* Get(util::DataHash, ILoadContext<L> const*);


	private:
		void OnEndOfFrame() override;
		void FreeDeferredReleases(uint64_t frameNum);


	private:
		template<typename F>
		friend class InvPtr;

		void Release(util::DataHash id);


	private:
		std::unordered_map<util::DataHash, PtrAndControl> m_ptrAndControlBlocks;
		mutable std::shared_mutex m_ptrAndControlBlocksMutex;

		// We defer resource release to avoid degenerate cases (e.g. release and then re-load the same thing)
		std::queue<std::pair<uint64_t, uint64_t>> m_deferredRelease; // <frame num, id>
		std::mutex m_deferredReleaseMutex;
		uint64_t m_currentFrameNum; // Relative to when this object was constructed
		static constexpr uint8_t k_deferredReleaseNumFrames = 1;
	};


	template<typename T>
	ResourceSystem<T>::ResourceSystem()
		: m_currentFrameNum(0)
	{
	}


	template<typename T>
	void ResourceSystem<T>::Destroy()
	{
		FreeDeferredReleases(std::numeric_limits<uint64_t>::max()); // Force-release everything

		{
			std::lock_guard<std::shared_mutex> readLock(m_ptrAndControlBlocksMutex);

			for (auto& entry : m_ptrAndControlBlocks)
			{
				SEAssert(entry.second.m_control->m_refCount.load() == 1 && entry.second.m_isPermanent == true,
					"ResourceSystem has outstanding InvPtr<T>s to be Release()'d. Only permanent objects should remain");

				entry.second.m_object->Destroy();
			}
			m_ptrAndControlBlocks.clear();
		}
	}


	template<typename T>
	bool ResourceSystem<T>::HasLoaded(util::DataHash id) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_ptrAndControlBlocksMutex);
			
			auto ptrCtrlItr = m_ptrAndControlBlocks.find(id);
			if (ptrCtrlItr != m_ptrAndControlBlocks.end())
			{
				return m_ptrAndControlBlocks.at(id).m_control->m_state.load() == ResourceState::Ready;
			}
			return false;
		}
	}


	template<typename T>
	template<typename L>
	ResourceSystem<T>::ControlBlock* ResourceSystem<T>::Get(util::DataHash id, ILoadContext<L> const* loadContext)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_ptrAndControlBlocksMutex);

			auto entryItr = m_ptrAndControlBlocks.find(id);
			if (entryItr != m_ptrAndControlBlocks.end())
			{
				return entryItr->second.m_control.get();
			}
		}

		// If we made it this far, we probably need to construct our object:
		{
			std::unique_lock<std::shared_mutex> write(m_ptrAndControlBlocksMutex);

			auto entryItr = m_ptrAndControlBlocks.find(id);
			if (entryItr != m_ptrAndControlBlocks.end()) // It might have been created while we waited
			{
				return entryItr->second.m_control.get();
			}
			else
			{
				SEAssert(loadContext != nullptr, 
					"Get() called with a null loadContext, this is only valid if the object is guaranteed to exist");

				ResourceSystem<T>::PtrAndControl& newPtrAndCntrl =
					m_ptrAndControlBlocks.emplace(id, ResourceSystem<T>::PtrAndControl{}).first->second;

				newPtrAndCntrl.m_control = std::make_unique<ControlBlock>();
				
				// The first InvPtr created will initialize the unique_ptr for us
				newPtrAndCntrl.m_control->m_object = &newPtrAndCntrl.m_object;

				newPtrAndCntrl.m_control->m_id = id;
				newPtrAndCntrl.m_control->m_owningResourceSystem = this;

				// The InvPtr will update these:
				newPtrAndCntrl.m_control->m_refCount.store(0);
				newPtrAndCntrl.m_control->m_state.store(ResourceState::Empty);

				// Bump the ref. count to keep permanent objects from going out of scope
				if (loadContext->m_isPermanent)
				{
					newPtrAndCntrl.m_control->m_refCount++;
					newPtrAndCntrl.m_isPermanent = true;
				}

				return newPtrAndCntrl.m_control.get();
			}
		}
	}


	template<typename T>
	void ResourceSystem<T>::OnEndOfFrame()
	{
		++m_currentFrameNum; // Increment the relative frame number each time this is called

		FreeDeferredReleases(m_currentFrameNum);
	}


	template<typename T>
	void ResourceSystem<T>::FreeDeferredReleases(uint64_t frameNum)
	{
		{
			std::scoped_lock lock(m_deferredReleaseMutex, m_ptrAndControlBlocksMutex);

			while (!m_deferredRelease.empty() &&
				m_deferredRelease.front().first + k_deferredReleaseNumFrames < frameNum)
			{
				SEAssert(m_ptrAndControlBlocks.contains(m_deferredRelease.front().second), "ID not found");

				// Allow resources to be resurrected from the deferred delete queue: Only actually destroy them if
				// their ref. count is 0
				if (m_ptrAndControlBlocks.at(m_deferredRelease.front().second).m_control->m_refCount.load() == 0)
				{
					SEAssert(m_ptrAndControlBlocks.at(m_deferredRelease.front().second).m_control->m_state.load() == 
						core::ResourceState::Released,
						"Ref count is 0, but state is not Released. This should not be possible");

					m_ptrAndControlBlocks.at(m_deferredRelease.front().second).m_object->Destroy();
					m_ptrAndControlBlocks.erase(m_deferredRelease.front().second);
				}

				m_deferredRelease.pop();
			}
		}
	}


	template<typename T>
	void ResourceSystem<T>::Release(util::DataHash ID)
	{
		{
			std::lock_guard<std::mutex> lock(m_deferredReleaseMutex);
			
			m_deferredRelease.emplace(m_currentFrameNum, ID);
		}
	}
}