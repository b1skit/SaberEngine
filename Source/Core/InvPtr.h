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

		bool operator==(InvPtr<T> const&) const noexcept;

		T& operator[](std::ptrdiff_t idx) const;

		explicit operator bool() const noexcept;

		~InvPtr();

		void Release() noexcept;

		core::ResourceSystem<T>::RefCountType UseCount() const;
		core::ResourceState GetState() const;

		bool IsValid() const; // Is this InvPtr Loaded/Loading?


	private:	
		core::ResourceSystem<T>::ControlBlock* m_control;


	private: // Use core::Inventory::Get()
		friend class Inventory;
		static InvPtr<T> Create(core::ResourceSystem<T>::ControlBlock*, std::shared_ptr<core::ILoadContext<T>>);
		explicit InvPtr(core::ResourceSystem<T>::ControlBlock*); // Create a new managed InvPtr
	};


	template<typename T>
	InvPtr<T>::InvPtr() noexcept
		: m_control(nullptr)
	{
	}


	template<typename T>
	InvPtr<T>::InvPtr(std::nullptr_t) noexcept
		: m_control(nullptr)
	{
	}


	template<typename T>
	InvPtr<T>::InvPtr(InvPtr<T> const& rhs) noexcept
		: m_control(nullptr)
	{
		*this = rhs;
	}


	template<typename T>
	InvPtr<T>::InvPtr(InvPtr<T>&& rhs) noexcept
		: m_control(nullptr)
	{
		*this = std::move(rhs);
	}


	template<typename T>
	InvPtr<T>& InvPtr<T>::operator=(InvPtr<T> const& rhs) noexcept
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
		}
		return *this;
	}


	template<typename T>
	InvPtr<T>& InvPtr<T>::operator=(InvPtr<T>&& rhs) noexcept
	{
		if (this != &rhs)
		{
			if (m_control && m_control->m_refCount.load() > 0) // Don't release brand new control blocks
			{
				Release(); // The InvPtr might already represent something, release first so we don't leak
			}

			m_control = rhs.m_control;
			rhs.m_control = nullptr;
		}
		return *this;
	}

	
	template<typename T>
	T* InvPtr<T>::operator->() const noexcept
	{
		SEAssert(IsValid(), "InvPtr is invalid");

		m_control->m_state.wait(ResourceState::Loading); // Block until the resource is loaded

		SEAssert(IsValid(), "InvPtr is invalid after loading");

		return static_cast<T*>(m_control->m_data);
	}


	template<typename T>
	T& InvPtr<T>::operator*() const noexcept
	{
		SEAssert(IsValid(), "InvPtr is invalid");

		m_control->m_state.wait(ResourceState::Loading); // Block until the resource is loaded

		SEAssert(IsValid(), "InvPtr is invalid after loading");

		return *static_cast<T*>(m_control->m_data);
	}


	template<typename T>
	bool InvPtr<T>::operator==(InvPtr<T> const& rhs) const noexcept
	{
		return m_control == rhs.m_control;
	}


	template<typename T>
	T& InvPtr<T>::operator[](std::ptrdiff_t idx) const
	{
		return operator->()[idx];
	}


	template<typename T>
	InvPtr<T>::operator bool() const noexcept
	{
		return m_control != nullptr;
	}


	template<typename T>
	InvPtr<T>::~InvPtr()
	{
		Release();
	}


	template<typename T>
	void InvPtr<T>::Release() noexcept
	{
		if (m_control) // If we're valid and our refcount is 0, free our memory:
		{
			SEAssert(m_control->m_refCount.load() > 0, "Ref count is about to underflow");

			if (--m_control->m_refCount == 0)
			{
				m_control->m_state.store(core::ResourceState::Released);
				m_control->m_owningResourceSystem->Release(m_control->m_id);
				m_control = nullptr; // The Release() will (deferred) delete this, null out our copy to invalidate ourselves
			}
		}
	}


	template<typename T>
	core::ResourceSystem<T>::RefCountType InvPtr<T>::UseCount() const
	{
		return m_control->m_refCount.load();
	}


	template<typename T>
	core::ResourceState InvPtr<T>::GetState() const
	{
		return m_control->m_state.load();
	}


	template<typename T>
	bool InvPtr<T>::IsValid() const
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
	InvPtr<T>::InvPtr(core::ResourceSystem<T>::ControlBlock* controlBlock)
		: m_control(controlBlock) // Note: The controlBlock may already be in use by other InvPtr<T>s
	{
		SEAssert(m_control, "Control cannot be null here");

		SEAssert(m_control->m_state.load() != core::ResourceState::Released || 
			m_control->m_refCount.load() == 0,
			"State is Released, but ref count is not 0. This should not be possible");

		// If the resource was Released, set its state back to Ready as it is still loaded
		core::ResourceState expected = core::ResourceState::Released;
		m_control->m_state.compare_exchange_strong(expected, core::ResourceState::Ready);

		m_control->m_refCount++;
	}


	template<typename T>
	InvPtr<T> InvPtr<T>::Create(
		core::ResourceSystem<T>::ControlBlock* control, std::shared_ptr<core::ILoadContext<T>> loadContext)
	{
		InvPtr<T> newInvPtr(control);

		// If we're in the Empty state, kick off an asyncronous loading job:
		core::ResourceState expected = core::ResourceState::Empty;
		if (newInvPtr.m_control->m_state.compare_exchange_strong(expected, core::ResourceState::Loading))
		{
			SEAssert(loadContext != nullptr, "Load context is null");

			core::ThreadPool::Get()->EnqueueJob([newInvPtr, loadContext]()
				{
					loadContext->OnLoadBegin(newInvPtr);

					// Populate the unique_ptr held by the ResourceSystem:
					*static_cast<std::unique_ptr<T>*>(newInvPtr.m_control->m_data) = loadContext->Load(newInvPtr);

					SEAssert(*static_cast<std::unique_ptr<T>*>(newInvPtr.m_control->m_data) != nullptr,
						"Load returned null");

					// Now the unique_ptr owning our object is created, swap our pointer to minimize indirection
					newInvPtr.m_control->m_data = static_cast<std::unique_ptr<T>*>(newInvPtr.m_control->m_data)->get();

					// Note: Our InvPtr is not yet marked as ready to ensure this call completes first
					loadContext->OnLoadComplete(newInvPtr);

					newInvPtr.m_control->m_state.store(core::ResourceState::Ready);

					newInvPtr.m_control->m_state.notify_all();
				});
		}

		return newInvPtr;
	}
}