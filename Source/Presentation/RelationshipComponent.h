// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "EntityManager.h"

#include "Core/Assert.h"


namespace fr
{
	class EntityManager;


	// A relationship is a doubly-linked list of entities
	class Relationship
	{
	public:
		static Relationship& AttachRelationshipComponent(EntityManager&, entt::entity owningEntity);

		void SetParent(EntityManager&, entt::entity);
		entt::entity GetParent() const;
		bool HasParent() const;

		entt::entity GetNext() const;
		entt::entity GetPrev() const;

		entt::entity GetFirstChild() const;
		entt::entity GetLastChild() const;

		entt::entity GetThisEntity() const;

		std::vector<entt::entity> GetAllDescendents() const;


	public:
		Relationship(Relationship&&) noexcept;
		Relationship& operator=(Relationship&&) noexcept;


	private:
		void AddChild(EntityManager&, entt::entity);
		void RemoveChild(EntityManager&, entt::entity);
		

	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		Relationship(PrivateCTORTag, entt::entity owningEntity);

		~Relationship();

		void Destroy();


	public:
		template<typename T>
		bool IsInHierarchyAbove() const; // Searches current entity and above

		template<typename T>
		T* GetFirstInHierarchyAbove() const; // Searches current entity and above

		template<typename T>
		T* GetFirstAndEntityInHierarchyAbove(entt::entity& owningEntityOut) const; // Searches current and above

		template<typename T>
		T* GetFirstInChildren() const;

		template<typename T>
		T* GetFirstAndEntityInChildren(entt::entity& childEntityOut) const; // Searches direct descendent children only (depth 1)

		template<typename T>
		std::vector<entt::entity> GetAllEntitiesInChildrenAndBelow() const; // Get all child entities with a T component


	private:
		entt::entity m_thisEntity;
		entt::entity m_parent;

		// Siblings
		entt::entity m_prev;
		entt::entity m_next;

		// Children
		entt::entity m_firstChild;
		entt::entity m_lastChild;

		bool m_isValid;

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


	inline bool Relationship::HasParent() const
	{
		return m_parent != entt::null;
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
	bool Relationship::IsInHierarchyAbove() const
	{
		return GetFirstInHierarchyAbove<T>() != nullptr;
	}


	template<typename T>
	T* Relationship::GetFirstInHierarchyAbove() const
	{
		entt::entity dummy = entt::null;
		return GetFirstAndEntityInHierarchyAbove<T>(dummy);
	}


	template<typename T>
	T* Relationship::GetFirstAndEntityInHierarchyAbove(entt::entity& owningEntityOut) const
	{
		fr::EntityManager* em = fr::EntityManager::Get();

		entt::entity currentEntity = m_thisEntity; // No lock needed: This should never change

		while (currentEntity != entt::null)
		{
			T* component = em->TryGetComponent<T>(currentEntity);
			if (component != nullptr)
			{
				owningEntityOut = currentEntity;
				return component;
			}

			fr::Relationship const& currentRelationship = em->GetComponent<fr::Relationship>(currentEntity);

			currentEntity = currentRelationship.GetParent();
		}
		owningEntityOut = entt::null;
		return nullptr;
	}


	template<typename T>
	T* Relationship::GetFirstInChildren() const
	{
		entt::entity dummy = entt::null;
		return GetFirstAndEntityInChildren<T>(dummy);
	}


	template<typename T>
	T* Relationship::GetFirstAndEntityInChildren(entt::entity& childEntityOut) const
	{
		fr::EntityManager* em = fr::EntityManager::Get();

		childEntityOut = entt::null;

		const entt::entity firstChild = GetFirstChild();
		entt::entity current = firstChild;
		do
		{
			fr::Relationship const& currentRelationship = em->GetComponent<fr::Relationship>(current);

			T* component = em->TryGetComponent<T>(current);
			if (component)
			{
				childEntityOut = current;
				return component;
			}

			current = currentRelationship.GetNext();
		} while (current != firstChild);

		return nullptr;
	}


	template<typename T>
	std::vector<entt::entity> Relationship::GetAllEntitiesInChildrenAndBelow() const
	{
		fr::EntityManager* em = fr::EntityManager::Get();

		std::vector<entt::entity> result;
		
		std::vector<entt::entity> const& allDescendents = GetAllDescendents();

		for (entt::entity cur : allDescendents)
		{
			if (em->HasComponent<T>(cur))
			{
				result.emplace_back(cur);
			}
		}

		return result;
	}
}