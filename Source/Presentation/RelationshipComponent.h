// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "EntityManager.h"

#include "Core/Assert.h"


namespace pr
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
		bool HasChildren() const;

		entt::entity GetThisEntity() const;

		std::vector<entt::entity> GetAllDescendents(EntityManager&) const; // Recursive: All entities in the parent/child hierarchy
		std::vector<entt::entity> GetAllChildren(EntityManager&) const; // Immediate children only (no descendents)


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

		void Destroy(pr::EntityManager&);


	public:
		template<typename T>
		bool IsInHierarchyAbove(pr::EntityManager& em) const; // Searches current entity and above

		template<typename T>
		T* GetFirstInHierarchyAbove(pr::EntityManager& em) const; // Searches current entity and above

		template<typename T>
		T* GetFirstAndEntityInHierarchyAbove(pr::EntityManager& em, entt::entity& owningEntityOut) const; // Searches current and above

		template<typename T, typename... Args>
		entt::entity GetFirstEntityInHierarchyAbove(pr::EntityManager& em) const;

		template<typename T>
		T* GetLastInHierarchyAbove(pr::EntityManager& em) const;

		template<typename T>
		T* GetLastAndEntityInHierarchyAbove(pr::EntityManager& em, entt::entity& owningEntityOut) const; // Keep searching until nothing is above

		template<typename T>
		T* GetFirstInChildren(pr::EntityManager& em) const;

		template<typename T>
		T* GetFirstAndEntityInChildren(pr::EntityManager& em, entt::entity& childEntityOut) const; // Searches direct descendent children only (depth 1)

		template<typename T, typename... Args>
		std::vector<entt::entity> GetAllEntitiesInChildrenAndBelow(pr::EntityManager& em) const; // Get all child entities with Type of

		template<typename T, typename... Args>
		std::vector<entt::entity> GetAllEntitiesInImmediateChildren(pr::EntityManager& em) const;

		template<typename T>
		uint32_t GetNumInImmediateChildren(pr::EntityManager& em) const;


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


	inline bool Relationship::HasChildren() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_relationshipMutex);
			SEAssert(m_firstChild != entt::null || m_firstChild == m_lastChild,
				"Children are out of sync");
			return m_firstChild != entt::null;
		}
	}


	inline entt::entity Relationship::GetThisEntity() const
	{
		return m_thisEntity;
	}


	template<typename T>
	bool Relationship::IsInHierarchyAbove(pr::EntityManager& em) const
	{
		return GetFirstInHierarchyAbove<T>(em) != nullptr;
	}


	template<typename T>
	T* Relationship::GetFirstInHierarchyAbove(pr::EntityManager& em) const
	{
		entt::entity dummy = entt::null;
		return GetFirstAndEntityInHierarchyAbove<T>(em, dummy);
	}


	template<typename T>
	T* Relationship::GetFirstAndEntityInHierarchyAbove(pr::EntityManager& em, entt::entity& owningEntityOut) const
	{
		entt::entity currentEntity = m_thisEntity; // No lock needed: This should never change

		while (currentEntity != entt::null)
		{
			T* component = em.TryGetComponent<T>(currentEntity);
			if (component != nullptr)
			{
				owningEntityOut = currentEntity;
				return component;
			}

			currentEntity = em.GetComponent<pr::Relationship>(currentEntity).GetParent();
		}
		owningEntityOut = entt::null;
		return nullptr;
	}


	template<typename T, typename... Args>
	entt::entity Relationship::GetFirstEntityInHierarchyAbove(pr::EntityManager& em) const
	{
		entt::entity currentEntity = m_thisEntity; // No lock needed: This should never change
		while (currentEntity != entt::null)
		{
			if (em.HasComponents<T, Args...>(currentEntity))
			{
				return currentEntity;
			}
			currentEntity = em.GetComponent<pr::Relationship>(currentEntity).GetParent();
		}
		return entt::null;
	}


	template<typename T>
	T* Relationship::GetLastInHierarchyAbove(pr::EntityManager& em) const
	{
		entt::entity dummy;
		return GetLastAndEntityInHierarchyAbove<T>(em, dummy);
	}


	template<typename T>
	T* Relationship::GetLastAndEntityInHierarchyAbove(pr::EntityManager& em, entt::entity& owningEntityOut) const
	{
		T* result = nullptr;
		owningEntityOut = entt::null;

		entt::entity curEntity = m_thisEntity;// No lock required: This should never change
		while (curEntity != entt::null)
		{
			if (T* curCmpt = em.TryGetComponent<T>(curEntity))
			{
				result = curCmpt;
				owningEntityOut = curEntity;
			}

			pr::Relationship const& curRelationship = em.GetComponent<pr::Relationship>(curEntity);
			curEntity = curRelationship.GetParent();
		}

		return result;
	}


	template<typename T>
	T* Relationship::GetFirstInChildren(pr::EntityManager& em) const
	{
		entt::entity dummy = entt::null;
		return GetFirstAndEntityInChildren<T>(em, dummy);
	}


	template<typename T>
	T* Relationship::GetFirstAndEntityInChildren(pr::EntityManager& em, entt::entity& childEntityOut) const
	{
		childEntityOut = entt::null;
		if (!HasChildren())
		{
			return nullptr;
		}

		childEntityOut = entt::null;

		const entt::entity firstChild = GetFirstChild();
		entt::entity current = firstChild;
		do
		{
			pr::Relationship const& currentRelationship = em.GetComponent<pr::Relationship>(current);

			T* component = em.TryGetComponent<T>(current);
			if (component)
			{
				childEntityOut = current;
				return component;
			}

			current = currentRelationship.GetNext();
		} while (current != firstChild);

		return nullptr;
	}


	template<typename T, typename... Args>
	std::vector<entt::entity> Relationship::GetAllEntitiesInChildrenAndBelow(pr::EntityManager& em) const
	{
		std::vector<entt::entity> const& allDescendents = GetAllDescendents();
		
		std::vector<entt::entity> result;
		result.reserve(allDescendents.size());

		for (entt::entity cur : allDescendents)
		{
			if (em.HasComponents<T, Args...>(cur))
			{
				result.emplace_back(cur);
			}
		}

		return result;
	}


	template<typename T, typename... Args>
	std::vector<entt::entity> Relationship::GetAllEntitiesInImmediateChildren(pr::EntityManager& em) const
	{
		std::vector<entt::entity> result;

		if (HasChildren())
		{
			const entt::entity firstChild = GetFirstChild();
			entt::entity curEntity = firstChild;
			do
			{
				if (em.HasComponents<T, Args...>(curEntity))
				{
					result.emplace_back(curEntity);
				}

				pr::Relationship const& curRelationship = em.GetComponent<pr::Relationship>(curEntity);
				curEntity = curRelationship.GetNext();
			} while (curEntity != firstChild);
		}

		return result;
	}


	template<typename T>
	uint32_t Relationship::GetNumInImmediateChildren(pr::EntityManager& em) const
	{
		uint32_t count = 0;

		{
			std::shared_lock<std::shared_mutex> readLock(m_relationshipMutex);

			entt::entity curChild = m_firstChild;
			while (curChild != entt::null)
			{
				if (em.HasComponent<T>(curChild))
				{
					++count;
				}

				pr::Relationship const& curRelationship = em.GetComponent<pr::Relationship>(curChild);
				curChild = curRelationship.GetNext();
				if (curChild == m_firstChild)
				{
					break;
				}
			}
		}

		return count;
	}
}