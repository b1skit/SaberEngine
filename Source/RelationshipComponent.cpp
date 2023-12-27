// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "RelationshipComponent.h"


namespace fr
{
	Relationship& Relationship::AttachRelationshipComponent(EntityManager& em, entt::entity owningEntity)
	{
		return *em.EmplaceComponent<fr::Relationship>(owningEntity, PrivateCTORTag{}, owningEntity);
	}


	Relationship::Relationship(PrivateCTORTag, entt::entity owningEntity)
		: m_thisEntity(owningEntity)
		, m_parent(entt::null)
		, m_prev(entt::null)
		, m_next(entt::null)
		, m_firstChild(entt::null)
		, m_lastChild(entt::null)
	{
	}


	Relationship::Relationship(Relationship&& rhs)  noexcept
		: m_thisEntity(rhs.m_thisEntity)
	{
		*this = std::move(rhs);
	}



	Relationship& Relationship::operator=(Relationship&& rhs) noexcept
	{
		{
			std::scoped_lock lock(m_relationshipMutex, rhs.m_relationshipMutex);

			m_parent = rhs.m_parent;
			rhs.m_parent = entt::null;

			m_prev = rhs.m_prev;
			rhs.m_prev = entt::null;

			m_next = rhs.m_next;
			rhs.m_next = entt::null;

			m_firstChild = rhs.m_firstChild;
			rhs.m_firstChild = entt::null;

			m_lastChild = rhs.m_lastChild;
			rhs.m_lastChild = entt::null;
			
			return *this;
		}
	}



	void Relationship::SetParent(EntityManager& em, entt::entity newParent)
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_relationshipMutex);

			SEAssert("Trying to set the same parent. This should be harmless, but it's unexpected", 
				newParent == entt::null || newParent != m_parent);

			if (m_parent != entt::null)
			{
				Relationship& prevParentRelationship = em.GetComponent<fr::Relationship>(m_parent);

				prevParentRelationship.RemoveChild(em, m_thisEntity);
			}

			// Update ourselves:
			m_parent = newParent;

			// Update the parent:
			if (newParent != entt::null)
			{
				Relationship& newParentRelationship = em.GetComponent<fr::Relationship>(newParent);
				newParentRelationship.AddChild(em, m_thisEntity);
			}
		}
	}


	void Relationship::AddChild(EntityManager& em, entt::entity newChild)
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_relationshipMutex);

			// Children are added to the end of our linked list
			Relationship& newChildRelationship = em.GetComponent<fr::Relationship>(newChild);

			SEAssert("Child should have already set this entity as its parent",
				newChildRelationship.m_parent == m_thisEntity);

			SEAssert("New child already has siblings",
				newChildRelationship.m_next == entt::null && newChildRelationship.m_prev == entt::null);

			if (m_firstChild == entt::null) // Adding a single node
			{
				SEAssert("Last child should also be null", m_lastChild == entt::null);

				newChildRelationship.m_next = newChild;
				newChildRelationship.m_prev = newChild;

				m_firstChild = newChild;
				m_lastChild = newChild;
			}
			else
			{
				Relationship& firstChildRelationship = em.GetComponent<fr::Relationship>(m_firstChild);
				Relationship& lastChildRelationship = em.GetComponent<fr::Relationship>(m_lastChild);

				SEAssert("Relationship linked list is corrupt: Last node does not point to the first node",
					lastChildRelationship.m_next == m_firstChild);

				lastChildRelationship.m_next = newChild;

				newChildRelationship.m_prev = lastChildRelationship.m_thisEntity;
				newChildRelationship.m_next = m_firstChild;

				firstChildRelationship.m_prev = newChild;

				m_lastChild = newChild;
			}
		}
	}


	void Relationship::RemoveChild(EntityManager& em, entt::entity child)
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_relationshipMutex);

			SEAssert("Trying to remove a child from a Relationship that has no children",
				m_firstChild != entt::null && m_lastChild != entt::null);

			Relationship& childRelationship = em.GetComponent<fr::Relationship>(child);

			bool foundChild = false;

			if (m_firstChild == m_lastChild) // Removing the only node
			{
				SEAssert("Trying to remove an entity that is not a child of the current Relationship",
					m_firstChild == child && m_lastChild == child);

				m_firstChild = entt::null;
				m_lastChild = entt::null;

				foundChild = true;
			}

			if (!foundChild)
			{
				entt::entity currentChild = m_firstChild;
				while (currentChild != m_lastChild)
				{
					Relationship& currentChildRelationship = em.GetComponent<fr::Relationship>(currentChild);

					if (child == currentChildRelationship.m_thisEntity)
					{
						Relationship& prevChildRelationship = 
							em.GetComponent<fr::Relationship>(currentChildRelationship.m_prev);
						Relationship& nextChildRelationship = 
							em.GetComponent<fr::Relationship>(currentChildRelationship.m_next);

						// Remove the node from the list:
						prevChildRelationship.m_next = nextChildRelationship.m_thisEntity;
						nextChildRelationship.m_prev = prevChildRelationship.m_thisEntity;

						foundChild = true;
						break;
					}
					currentChild = currentChildRelationship.m_next;
				}

				// Handle the last node:
				if (!foundChild && child == m_lastChild)
				{
					Relationship& lastChildRelationship = em.GetComponent<fr::Relationship>(m_lastChild);

					Relationship& prevChildRelationship = 
						em.GetComponent<fr::Relationship>(lastChildRelationship.m_prev);
					SEAssert("Last child's next should be the first child", 
						lastChildRelationship.m_next == m_firstChild);

					Relationship& firstChildRelationship = em.GetComponent<fr::Relationship>(m_firstChild);

					prevChildRelationship.m_next = m_firstChild;
					firstChildRelationship.m_prev = prevChildRelationship.m_thisEntity;

					foundChild = true;
				}
			}

			// Finally, cleanup the child's linked list references:
			SEAssert("Could not find the child to remove", foundChild == true);
			childRelationship.m_prev = entt::null;
			childRelationship.m_next = entt::null;
		}
	}
}