// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
#include "ResourceSystem.h"
#include "ThreadPool.h"


namespace core
{
	template<typename T>
	class InvPtr
	{
	public:
		InvPtr() noexcept; // Empty/invalid 
		InvPtr(std::nullptr_t) noexcept;

		InvPtr(InvPtr<T> const&) noexcept;
		InvPtr(InvPtr<T>&&) noexcept;

		InvPtr& operator=(InvPtr<T> const&) noexcept;
		InvPtr& operator=(InvPtr<T>&&) noexcept;

		T* operator->() const noexcept;
		T& operator*() const noexcept;
		T& operator[](std::ptrdiff_t idx) const;

		bool operator==(InvPtr<T> const&) const noexcept;

		explicit operator bool() const noexcept;

		~InvPtr();

		void Release() noexcept;

		template<typename Child>
		InvPtr<Child>& AddDependency(InvPtr<Child>&);

		template<typename Child>
		InvPtr<Child> AddDependency(InvPtr<Child>&&);

		core::ResourceSystem<T>::RefCountType UseCount() const;
		core::ResourceState GetState() const;

		bool IsValid() const; // Is this InvPtr Loaded/Loading?


	private:
		// A locally cached copy of the control block object to minimize indirection. Must be mutable as we can only
		// update it after a resource has finished loading and is in the Ready state
		mutable T* m_objectCache;
		core::ResourceSystem<T>::ControlBlock* m_control;


	private: // Use core::Inventory::Get()
		friend class Inventory;
		static InvPtr<T> Create(core::ResourceSystem<T>::ControlBlock*, std::shared_ptr<core::ILoadContext<T>>);
		explicit InvPtr(core::ResourceSystem<T>::ControlBlock*); // Create a new managed InvPtr

		template<typename Dependency>
		friend class InvPtr;
	};


	template<typename T>
	inline InvPtr<T>::InvPtr() noexcept
		: m_objectCache(nullptr)
		, m_control(nullptr)
	{
	}


	template<typename T>
	inline InvPtr<T>::InvPtr(std::nullptr_t) noexcept
		: m_objectCache(nullptr)
		, m_control(nullptr)
	{
	}


	template<typename T>
	inline InvPtr<T>::InvPtr(InvPtr<T> const& rhs) noexcept
		: m_objectCache(nullptr)
		, m_control(nullptr)
	{
		*this = rhs;
	}


	template<typename T>
	inline InvPtr<T>::InvPtr(InvPtr<T>&& rhs) noexcept
		: m_objectCache(nullptr)
		, m_control(nullptr)
	{
		*this = std::move(rhs);
	}


	template<typename T>
	inline InvPtr<T>& InvPtr<T>::operator=(InvPtr<T> const& rhs) noexcept
	{
		if (this != &rhs) // It is possible rhs is invalid, but we still want to copy whatever it contains
		{
			if (m_control && m_control->m_refCount.load() > 0) // Don't release brand new control blocks
			{
				Release(); // The InvPtr might already represent something, release first so we don't leak
			}

			m_control = rhs.m_control;
			if (m_control)
			{
				SEAssert(m_control->m_refCount.load() < std::numeric_limits<core::ResourceSystem<T>::RefCountType>::max(),
					"Ref count is about to overflow");

				m_control->m_refCount++;
			}

			m_objectCache = rhs.m_objectCache;
		}
		return *this;
	}


	template<typename T>
	inline InvPtr<T>& InvPtr<T>::operator=(InvPtr<T>&& rhs) noexcept
	{
		if (this != &rhs)
		{
			if (m_control && m_control->m_refCount.load() > 0) // Don't release brand new control blocks
			{
				Release(); // The InvPtr might already represent something, release first so we don't leak
			}

			m_control = rhs.m_control;
			rhs.m_control = nullptr;

			m_objectCache = rhs.m_objectCache;
			rhs.m_objectCache = nullptr;

		}
		return *this;
	}

	
	template<typename T>
	inline T* InvPtr<T>::operator->() const noexcept
	{
		SEAssert(IsValid(), "InvPtr is invalid");

		if (m_objectCache)
		{
			return m_objectCache;
		}

		m_control->m_state.wait(ResourceState::Loading); // Block until the resource is loaded
		SEAssert(IsValid() && m_control->m_object && m_control->m_object->get(), "InvPtr is invalid after loading");
		
		// Update the local cache of the object pointer, now that loading has finished:
		SEAssert(m_objectCache == nullptr, "Cached object pointer should be null until the resource has loaded");
		m_objectCache = m_control->m_object->get();

		return m_objectCache;
	}


	template<typename T>
	inline T& InvPtr<T>::operator*() const noexcept
	{
		return *operator->();
	}


	template<typename T>
	inline T& InvPtr<T>::operator[](std::ptrdiff_t idx) const
	{
		return operator->()[idx];
	}


	template<typename T>
	inline bool InvPtr<T>::operator==(InvPtr<T> const& rhs) const noexcept
	{
		return m_control == rhs.m_control;
	}


	template<typename T>
	inline InvPtr<T>::operator bool() const noexcept
	{
		return m_control != nullptr;
	}


	template<typename T>
	inline InvPtr<T>::~InvPtr()
	{
		Release();
	}


	template<typename T>
	inline void InvPtr<T>::Release() noexcept
	{
		if (m_control) // If we're valid and our refcount is 0, free our memory:
		{
			SEAssert(m_control->m_refCount.load() > 0, "Ref count is about to underflow");

			if (--m_control->m_refCount == 0)
			{
				SEAssert(m_control->m_loadContext == nullptr, "Load context is not null. This should not be possible");

				m_control->m_state.store(core::ResourceState::Released);
				m_control->m_owningResourceSystem->Release(m_control->m_id);
				m_control = nullptr; // The Release() will (deferred) delete this, null out our copy to invalidate ourselves

				m_objectCache = nullptr;
			}
		}
	}

	template<typename T>
	template<typename Child>
	InvPtr<Child>& InvPtr<T>::AddDependency(InvPtr<Child>& child)
	{
		SEAssert(IsValid() && child.IsValid(), "Cannot add dependencies to invalid InvPtrs");

		// Add our callback to the child:
		{
			std::scoped_lock lock(m_control->m_loadContextMutex, child.m_control->m_loadContextMutex);

			SEAssert(child.m_control->m_loadContext ||
				child.m_control->m_state.load() == ResourceState::Ready,
				"Trying to add a null load context as a child dependency, this should only be possible if it is Ready");

			// Add our callback to the child:
			if (child.m_control->m_loadContext && // If the child has no load context, it must be Ready
				child.m_control->m_state.load() != ResourceState::Ready) // It might be ready, but not have cleared its load context yet
			{
				ILoadContextBase::CreateLoadDependency(m_control->m_loadContext, child.m_control->m_loadContext);
			}
		}

		return child;
	}


	template<typename T>
	template<typename Child>
	InvPtr<Child> InvPtr<T>::AddDependency(InvPtr<Child>&& child)
	{
		SEAssert(IsValid() && child.IsValid(), "Cannot add dependencies to invalid InvPtrs");

		{
			std::scoped_lock lock(m_control->m_loadContextMutex, child.m_control->m_loadContextMutex);

			SEAssert(child.m_control->m_loadContext ||
				child.m_control->m_state.load() == ResourceState::Ready,
				"Trying to add a null load context as a child dependency, this should only be possible if it is Ready");

			// Add our callback to the child:
			if (child.m_control->m_loadContext && // If the child has no load context, it must be Ready
				child.m_control->m_state.load() != ResourceState::Ready) // It might be ready, but not have cleared its load context yet
			{
				ILoadContextBase::CreateLoadDependency(m_control->m_loadContext, child.m_control->m_loadContext);
			}
		}

		return std::move(child);
	}


	template<typename T>
	inline core::ResourceSystem<T>::RefCountType InvPtr<T>::UseCount() const
	{
		return m_control->m_refCount.load();
	}


	template<typename T>
	inline core::ResourceState InvPtr<T>::GetState() const
	{
		return m_control->m_state.load();
	}


	template<typename T>
	inline bool InvPtr<T>::IsValid() const
	{
		if (m_control != nullptr)
		{
			const core::ResourceState currentState = m_control->m_state.load();

			return currentState != core::ResourceState::Empty &&
				currentState != core::ResourceState::Released &&
				currentState != core::ResourceState::Error;
		}

		return false;
	}


	template<typename T>
	inline InvPtr<T>::InvPtr(core::ResourceSystem<T>::ControlBlock* controlBlock)
		: m_control(controlBlock) // Note: The controlBlock may already be in use by other InvPtr<T>s
	{
		SEAssert(m_control, "Control cannot be null here");
		SEAssert(m_control->m_object != nullptr, "Control object pointer cannot be null here");

		SEAssert(m_control->m_state.load() != core::ResourceState::Released || 
			m_control->m_refCount.load() == 0,
			"State is Released, but ref count is not 0. This should not be possible");

		// If the resource was Released, set its state back to Ready as it is still loaded
		core::ResourceState expected = core::ResourceState::Released;
		if (m_control->m_state.compare_exchange_strong(expected, core::ResourceState::Ready))
		{
			// If the object was released, we need to re-set the local cache of the object pointer
			m_objectCache = m_control->m_object->get();
		}

		m_control->m_refCount++;
	}


	template<typename T>
	inline InvPtr<T> InvPtr<T>::Create(
		core::ResourceSystem<T>::ControlBlock* control, std::shared_ptr<core::ILoadContext<T>> loadContext)
	{
		InvPtr<T> newInvPtr(control);

		// If we're in the Empty state, kick off an asyncronous loading job:
		core::ResourceState expected = core::ResourceState::Empty;
		if (newInvPtr.m_control->m_state.compare_exchange_strong(expected, core::ResourceState::Loading))
		{
			SEAssert(loadContext != nullptr, "Load context is null");

			// Initialize and store our own load context: We use this for dependency callbacks
			{
				std::lock_guard<std::mutex> lock(newInvPtr.m_control->m_loadContextMutex);

				loadContext->Initialize(newInvPtr.m_control->m_id);
				newInvPtr.m_control->m_loadContext = loadContext;
			}

			// Do this on the current thread; guarantees the InvPtr can be registered with any systems that might
			// require it before the creation can possibly have finished
			loadContext->OnLoadBegin(newInvPtr); 

			core::ThreadPool::Get()->EnqueueJob([newInvPtr]()
				{
					SEAssert(newInvPtr.m_control->m_object != nullptr && newInvPtr.m_control->m_object->get() == nullptr,
						"Pointer should refer to an empty unique pointer here");

					// Populate the unique_ptr held by the ResourceSystem.
					// Note: We don't lock m_loadContextMutex here, it will be locked internally to add dependencies
					*newInvPtr.m_control->m_object =
						std::dynamic_pointer_cast<ILoadContext<T>>(newInvPtr.m_control->m_loadContext)->Load(newInvPtr);

					if (*newInvPtr.m_control->m_object == nullptr)
					{
						newInvPtr.m_control->m_state.store(core::ResourceState::Error);

						SEAssertF("Resource loading error. TODO: Handle dependencies now that we have an error state");
					}
					else
					{
						// The unique_ptr owning our object is created: Swap our pointer to minimize indirection
						newInvPtr.m_objectCache = newInvPtr.m_control->m_object->get();

						// We're done! Mark ourselves as ready, and notify anybody waiting on us
						newInvPtr.m_control->m_state.store(core::ResourceState::Ready);
						
					}
					newInvPtr.m_control->m_state.notify_all();
					

					// Finally, handle dependencies:
					{
						std::lock_guard<std::mutex> lock(newInvPtr.m_control->m_loadContextMutex);

						// The last thread through here will execute ILoadContextBase::OnLoadComplete, and call
						// back to any parents
						newInvPtr.m_control->m_loadContext->Finalize();

						// Release load context: No need for this anymore. Any children with a copy will keep this alive
						// until it is no longer needed
						newInvPtr.m_control->m_loadContext = nullptr;
					}
				});
		}

		return newInvPtr;
	}
}