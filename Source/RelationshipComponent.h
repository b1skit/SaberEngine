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

		entt::entity GetThisEntity() const;


	public:
		template<typename T>
		static bool IsInHierarchyAbove(entt::entity); // Searches current entity and above

		template<typename T>
		static T* GetFirstInHierarchyAbove(entt::entity); // Searches current entity and above

		template<typename T>
		static T* GetFirstAndEntityInHierarchyAbove(entt::entity, entt::entity& owningEntityOut); // Searches current entity and above


	public:
		Relationship(Relationship&&) noexcept;
		Relationship& operator=(Relationship&&) noexcept;


	private:
		void AddChild(GameplayManager&, entt::entity);
		void RemoveChild(GameplayManager&, entt::entity);
		

	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		Relationship(PrivateCTORTag, entt::entity owningEntity);


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


	inline entt::entity Relationship::GetThisEntity() const
	{
		return m_thisEntity;
	}


	template<typename T>
	bool Relationship::IsInHierarchyAbove(entt::entity entity)
	{
		return GetFirstInHierarchyAbove<T>(entity) != nullptr;
	}


	template<typename T>
	T* Relationship::GetFirstInHierarchyAbove(entt::entity entity)
	{
		entt::entity dummy = entt::null;
		return GetFirstAndEntityInHierarchyAbove<T>(entity, dummy);
	}


	template<typename T>
	T* Relationship::GetFirstAndEntityInHierarchyAbove(entt::entity entity, entt::entity& owningEntityOut)
	{
		SEAssert("Entity cannot be null", entity != entt::null);

		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		entt::entity currentEntity = entity;
		while (currentEntity != entt::null)
		{
			T* component = gpm.TryGetComponent<T>(currentEntity);
			if (component != nullptr)
			{
				owningEntityOut = currentEntity;
				return component;
			}

			SEAssert("Current entity does not have a Relationship component",
				gpm.HasComponent<fr::Relationship>(currentEntity));
			fr::Relationship const& currentRelationship = gpm.GetComponent<fr::Relationship>(currentEntity);

			currentEntity = currentRelationship.GetParent();
		}

		return nullptr;
	}
}