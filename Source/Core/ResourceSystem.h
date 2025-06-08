// � 2024 Adam Badke. All rights reserved.
#pragma once
#include "Interfaces/ILoadContext.h"

#include "Util/HashKey.h"


namespace core
{
	enum class ResourceState : uint8_t
	{
		Empty,
		Requested,
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
			std::shared_ptr<ILoadContextBase> m_loadContext; // For dependency management
			std::mutex m_loadContextMutex;

			std::unique_ptr<T>* m_object = nullptr; // The InvPtr populates our unique_ptr asyncronously

			util::HashKey m_id = 0;
			ResourceSystem<T>* m_owningResourceSystem = nullptr;

			alignas(CACHE_LINE_SIZE) std::atomic<RefCountType> m_refCount = 0;
			alignas(CACHE_LINE_SIZE) std::atomic<ResourceState> m_state = ResourceState::Empty;
		};
#undef CACHE_LINE_SIZE

		struct PtrAndControl
		{
			std::unique_ptr<T> m_object;
			std::unique_ptr<ControlBlock> m_control;
			
			RetentionPolicy m_retentionPolicy;
		};
		

	public:
		ResourceSystem();

		ResourceSystem(ResourceSystem const&) = default;
		ResourceSystem(ResourceSystem&&) = default;

		ResourceSystem& operator=(ResourceSystem const&) = default;
		ResourceSystem& operator=(ResourceSystem&&) = default;

		~ResourceSystem();

		void Destroy() override;


	public:
		bool HasLoaded(util::HashKey) const;
		bool Has(util::HashKey) const;

		template<typename L>
		ControlBlock* Get(util::HashKey, std::shared_ptr<ILoadContext<L>> const&);


	private:
		void OnEndOfFrame() override;
		void FreeDeferredReleases(uint64_t frameNum);


	private:
		template<typename F>
		friend class InvPtr;

		void Release(util::HashKey id);


	private:
		std::unordered_map<util::HashKey, PtrAndControl> m_ptrAndControlBlocks;
		mutable std::shared_mutex m_ptrAndControlBlocksMutex;

		// We defer resource release to avoid degenerate cases (e.g. release and then re-load the same thing). 
		// Note: This is not intended to guarantee resource lifetime/scope, it is only a reload optimization
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
	ResourceSystem<T>::~ResourceSystem()
	{
		// Due to indeterminate ordering when Destroy() is called, we must check for resource leaks here, once we know
		// all ResourceSystems have destroyed their contents
#if defined (_DEBUG)
		for (auto& entry : m_ptrAndControlBlocks)
		{
			const RefCountType entryRefCount = entry.second.m_control->m_refCount.load();

			SEAssert(entryRefCount == 0 ||
				(entryRefCount == 1 && entry.second.m_retentionPolicy == core::RetentionPolicy::Permanent),
				"There is an outstanding InvPtr that has not been released yet. This might indicate a resource leak");
		}
#endif
	}


	template<typename T>
	void ResourceSystem<T>::Destroy()
	{
		FreeDeferredReleases(std::numeric_limits<uint64_t>::max()); // Force-release everything

		{
			std::lock_guard<std::shared_mutex> readLock(m_ptrAndControlBlocksMutex);

			for (auto& entry : m_ptrAndControlBlocks)
			{
				// Note: Resources may still have a ref count >= 1 here, as they may be permanent or still referenced
				// by another resource held another ResourceSystem that has not been Destroy()ed yet
				entry.second.m_object->Destroy();
			}
			// Note: For the same reason as above, we can't clear m_ptrAndControlBlocks here: A Resource held by an
			// InvPtr might contain other InvPtrs. If we clear m_ptrAndControlBlocks, they won't be able to destroy
			// themselves
		}
	}


	template<typename T>
	bool ResourceSystem<T>::HasLoaded(util::HashKey id) const
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
	bool ResourceSystem<T>::Has(util::HashKey id) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_ptrAndControlBlocksMutex);

			auto ptrCtrlItr = m_ptrAndControlBlocks.find(id);
			if (ptrCtrlItr != m_ptrAndControlBlocks.end())
			{
				const ResourceState resourceState = m_ptrAndControlBlocks.at(id).m_control->m_state.load();

				// Note: We cannot say we have a resource if it is in the Empty state, as this allows a race condition
				// where a thread that does not supply a load context might transition the resource state to Requested
				// but not be able to load it
				return resourceState == ResourceState::Requested ||
					resourceState == ResourceState::Loading ||
					resourceState == ResourceState::Ready;
			}
			return false;
		}
	}


	template<typename T>
	template<typename L>
	ResourceSystem<T>::ControlBlock* ResourceSystem<T>::Get(
		util::HashKey id, std::shared_ptr<ILoadContext<L>> const& loadContext)
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
			std::unique_lock<std::shared_mutex> writeLock(m_ptrAndControlBlocksMutex);

			auto entryItr = m_ptrAndControlBlocks.find(id);
			if (entryItr != m_ptrAndControlBlocks.end()) // It might have been created while we waited
			{
				return entryItr->second.m_control.get();
			}
			else
			{
				// Note: There's a race condition here if 2 resources are created at the same time with different load
				// contexts: The first one will set the load context, and the second one will be ignored. If this
				// becomes an issue, we should implement operator==() for our load contexts and assert on equality here
				// to catch it

				SEAssert(loadContext != nullptr, 
					"Get() called with a null loadContext, this is only valid if the object is guaranteed to exist");

				ResourceSystem<T>::PtrAndControl& newPtrAndCntrl =
					m_ptrAndControlBlocks.emplace(id, ResourceSystem<T>::PtrAndControl{}).first->second;

				newPtrAndCntrl.m_control = std::make_unique<ControlBlock>();

				newPtrAndCntrl.m_control->m_loadContext = loadContext; // InvPtr calls ILoadContextBase::Initialize()
				
				// The first InvPtr created will initialize the unique_ptr for us
				newPtrAndCntrl.m_control->m_object = &newPtrAndCntrl.m_object;

				newPtrAndCntrl.m_control->m_id = id;
				newPtrAndCntrl.m_control->m_owningResourceSystem = this;

				// The InvPtr will update these:
				newPtrAndCntrl.m_control->m_refCount.store(0);
				newPtrAndCntrl.m_control->m_state.store(ResourceState::Empty);

				newPtrAndCntrl.m_retentionPolicy = loadContext->m_retentionPolicy;

				// Bump the ref. count to keep permanent objects from going out of scope
				if (newPtrAndCntrl.m_retentionPolicy == core::RetentionPolicy::Permanent)
				{
					newPtrAndCntrl.m_control->m_refCount++;
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
				const util::HashKey ID = m_deferredRelease.front().second;

				// It is possible for Resources to be added to the deferred delete queue multiple times (e.g. if they're
				// resurrected/released multiple times), the important thing is that they have a ref count of zero for
				// the entry when we actually free them
				auto entryItr = m_ptrAndControlBlocks.find(ID);
				if (entryItr != m_ptrAndControlBlocks.end())
				{
					PtrAndControl& ptrAndCtrl = entryItr->second;
					if (ptrAndCtrl.m_control->m_refCount.load() == 0)
					{
						SEAssert(ptrAndCtrl.m_control->m_state.load() == core::ResourceState::Released,
							"Ref count is 0, but state is not Released. This should not be possible");

						ptrAndCtrl.m_object->Destroy();
						m_ptrAndControlBlocks.erase(ID);
					}
				}				

				m_deferredRelease.pop();
			}
		}
	}


	template<typename T>
	void ResourceSystem<T>::Release(util::HashKey ID)
	{
		bool immediatelyReleased = false;
		{
			std::unique_lock<std::shared_mutex> writeLock(m_ptrAndControlBlocksMutex);

			SEAssert(m_ptrAndControlBlocks.contains(ID),
				"Trying to release an ID that doesn't exist. This should not be possible");

			if (m_ptrAndControlBlocks.at(ID).m_retentionPolicy == core::RetentionPolicy::ForceNew)
			{
				SEAssert(m_ptrAndControlBlocks.at(ID).m_control->m_refCount.load() == 0 &&
					m_ptrAndControlBlocks.at(ID).m_control->m_state.load() == core::ResourceState::Released,
					"Immediately-released resources must have a ref. count of 0 and Released state");

				m_ptrAndControlBlocks.at(ID).m_object->Destroy();
				m_ptrAndControlBlocks.erase(ID);

				immediatelyReleased = true;
			}
		}

		if (!immediatelyReleased)
		{
			std::lock_guard<std::mutex> lock(m_deferredReleaseMutex);

			m_deferredRelease.emplace(m_currentFrameNum, ID);
		}
	}
}