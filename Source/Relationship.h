// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace fr
{
	class GameplayManager;


	// A relationship is a doubly-linked list of entities
	class Relationship
	{
	public:
		static Relationship& AttachRelationshipComponent(GameplayManager&, entt::entity owningEntity);

		void SetParent(GameplayManager&, entt::entity);
		entt::entity GetParent() const;
		
		entt::entity GetNext() const;
		entt::entity GetPrev() const;

		entt::entity GetFirstChild() const;
		entt::entity GetLastChild() const;


	public:
		template<typename T>
		static bool HasComponentInParentHierarchy(entt::entity);

		template<typename T>
		static T* GetComponentInHierarchyAbove(entt::entity);


	public:
		Relationship(Relationship&&) noexcept;
		Relationship& operator=(Relationship&&) noexcept;


	private:
		void AddChild(GameplayManager&, entt::entity);
		void RemoveChild(GameplayManager&, entt::entity);
		

	private: // Use AttachRelationshipComponent() instead
		Relationship(entt::entity owningEntity);


	private:
		const entt::entity m_thisEntity;
		entt::entity m_parent;

		// Siblings
		entt::entity m_prev;
		entt::entity m_next;

		// Children
		entt::entity m_firstChild;
		entt::entity m_lastChild;

		mutable std::shared_mutex m_relationshipMutex;


	private: // No copying allowed
		Relationship() = delete;
		Relationship(Relationship const&) = delete;
		Relationship& operator=(Relationship const&) = delete;
	};


	inline entt::entity Relationship::GetParent() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_relationshipMutex);
			return m_parent;
		}
	}


	inline entt::entity Relationship::GetNext() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_relationshipMutex);
			return m_next;
		}
	}


	inline entt::entity Relationship::GetPrev() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_relationshipMutex);
			return m_prev;
		}
	}


	inline entt::entity Relationship::GetFirstChild() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_relationshipMutex);
			return m_firstChild;
		}
	}


	inline entt::entity Relationship::GetLastChild() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_relationshipMutex);
			return m_lastChild;
		}
	}


	template<typename T>
	bool Relationship::HasComponentInParentHierarchy(entt::entity entity)
	{
		return GetComponentInHierarchyAbove<T>(entity) != nullptr;
	}


	template<typename T>
	T* Relationship::GetComponentInHierarchyAbove(entt::entity entity)
	{
		SEAssert("Entity cannot be null", entity != entt::null);

		fr::GameplayManager& gpm = *fr::GameplayManager::Get();
		
		entt::entity currentEntity = entity;
		while (currentEntity != entt::null)
		{
			T* component = gpm.TryGetComponent<T>(currentEntity);
			if (component != nullptr)
			{
				return component;
			}

			SEAssert("Current entity does not have a Relationship component",
				gpm.HasComponent<fr::Relationship>(currentEntity));
			fr::Relationship const& currentRelationship = gpm.GetComponent<fr::Relationship>(currentEntity);

			currentEntity = currentRelationship.GetParent();
		}

		SEAssertF("Component not found in current entity, or the parents above it");

		return nullptr;
	}
}