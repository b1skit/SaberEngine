// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "../Util/HashKey.h"


namespace core
{
	template<typename T>
	class InvPtr;


	enum class RetentionPolicy : uint8_t
	{
		Reusable,	// Default: Resource can be resurrected/reused after Release if the delete has not occurred yet
		ForceNew,	// Resource cannot be resurrected/reused after Release. A new Resource must be created/loaded
		Permanent,	// Resource is permanent and cannot be deleted
	};


	struct ILoadContextBase
	{
	public:
		virtual ~ILoadContextBase() = default;


	public:
		inline virtual void CallOnLoadComplete() = 0;

	protected: // InvPtr interface:
		template<typename T>
		friend class InvPtr;

		static void CreateLoadDependency(
			std::shared_ptr<ILoadContextBase>& parentLoadCtx,
			std::shared_ptr<ILoadContextBase>& childLoadCtx);

		void Initialize(util::HashKey objectID);
		void Finalize();


	private:
		void FinalizeDependencies(util::HashKey);

		std::unordered_set<util::HashKey> m_childDependencies; // We need to wait until these notify us they're done
		std::mutex m_childDependenciesMutex;

		std::vector<std::shared_ptr<ILoadContextBase>> m_parentLoadContexts; // We'll notify these when we're done loading
		std::mutex m_parentLoadContextsMutex;

		util::HashKey m_objectID; // ID of the object associated with this instance
	};


	// Visitor interface: Inherit from this to handle specific loading cases
	template<typename T>
	struct ILoadContext : public virtual ILoadContextBase
	{
	protected: // InvPtr interface:
		friend class InvPtr<T>;

		inline void Initialize(util::HashKey objectID, InvPtr<T> const& invPtr)
		{
			ILoadContextBase::Initialize(objectID);
			m_invPtr = invPtr;
		}

		inline void					CallOnLoadBegin()				{ OnLoadBegin(m_invPtr); }
		inline std::unique_ptr<T>	CallLoad()						{ return Load(m_invPtr); }
		inline void					CallOnLoadComplete() override	{ OnLoadComplete(m_invPtr); }

		InvPtr<T> m_invPtr;


	public:
		virtual ~ILoadContext() = default;

		RetentionPolicy m_retentionPolicy = RetentionPolicy::Reusable;


	public: // Virtual interface: Implement as necessary

		// Optional: Executed on the calling thread before any async load work is kicked off. Use this to notify any 
		// systems that might need a copy of the InvPtr immediately
		inline virtual void OnLoadBegin(core::InvPtr<T>&) {};

		// Async: The bulk of the loading and creation should be done here. Returning nullptr signals a loading error
		inline virtual std::unique_ptr<T> Load(core::InvPtr<T>&) = 0;

		// Optional: Handle any post-loading steps here. Called by whatever thread loaded the last dependency
		// Note: The ResourceState will already be Ready by this point, do not use this for anything that must be done
		// before waiting threads are unblocked
		inline virtual void OnLoadComplete(core::InvPtr<T>&) {};
	};


	inline void ILoadContextBase::CreateLoadDependency(
		std::shared_ptr<ILoadContextBase>& parentLoadCtx,
		std::shared_ptr<ILoadContextBase>& childLoadCtx)
	{
		SEAssert(parentLoadCtx && childLoadCtx, "A load context is null");

		{
			std::scoped_lock lock(parentLoadCtx->m_childDependenciesMutex, childLoadCtx->m_parentLoadContextsMutex);

			// Only add unique dependencies
			if (parentLoadCtx->m_childDependencies.emplace(childLoadCtx->m_objectID).second)
			{
				childLoadCtx->m_parentLoadContexts.emplace_back(parentLoadCtx);
			}
		}
	};


	inline void ILoadContextBase::Initialize(util::HashKey objectID)
	{
		m_objectID = objectID;

		// We add ourselves as a child dependency, to prevent a race condition where a child finishes before we do
		// and begins the finalization process
		{
			std::lock_guard lock(m_childDependenciesMutex);
			m_childDependencies.emplace(m_objectID);
		}
	}


	inline void ILoadContextBase::Finalize()
	{
		FinalizeDependencies(m_objectID); // We added ourself as a child dependency, so clear it here
	}


	inline void ILoadContextBase::FinalizeDependencies(util::HashKey childID)
	{
		{
			std::lock_guard<std::mutex> lock(m_childDependenciesMutex);

			SEAssert(m_childDependencies.contains(childID),
				"Child ID is not registered as a dependent. This should not be possible");
			
			m_childDependencies.erase(childID);

			if (m_childDependencies.empty()) // This thread must be constructing the last child to complete
			{
				// We're done! Execute any remaining post-processing work:
				CallOnLoadComplete();

				// Notify any parent waiting on us to complete:
				{
					std::lock_guard<std::mutex> lock(m_parentLoadContextsMutex);

					for (size_t parentIdx = 0; parentIdx < m_parentLoadContexts.size(); ++parentIdx)
					{
						m_parentLoadContexts[parentIdx]->FinalizeDependencies(m_objectID);
					}
					m_parentLoadContexts.clear(); // Finally, free our parent load contexts
				}
			}
		}
	}
}