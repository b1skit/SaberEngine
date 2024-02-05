// � 2023 Adam Badke. All rights reserved.
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


	Relationship::~Relationship()
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		// When destroying an entity, EnTT does a swap_and_pop that results in our Relationship DTOR being called twice,
		// back-to-back for the same entity. To side-step this issue, use TryGetComponent
		fr::Relationship* relationship = em.TryGetComponent<fr::Relationship>(m_thisEntity);
		if (relationship)
		{
			relationship->SetParent(em, entt::null);

			entt::entity curChild = relationship->GetFirstChild();
			while (curChild != entt::null)
			{
				fr::Relationship& childRelationship = em.GetComponent<fr::Relationship>(curChild);
				childRelationship.SetParent(em, entt::null);

				curChild = childRelationship.GetNext();
			}
		}
	}


	Relationship& Relationship::operator=(Relationship&& rhs) noexcept
	{
		if (this == &rhs)
		{
			return *this;
		}

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
		SEAssert(newParent == entt::null || newParent != m_parent,
			"Trying to set the same parent. This should be harmless, but it's unexpected");

		{
			std::unique_lock<std::shared_mutex> writeLock(m_relationshipMutex);
			
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

			SEAssert(newChildRelationship.m_parent == m_thisEntity,
				"Child should have already set this entity as its parent");

			SEAssert(newChildRelationship.m_next == entt::null && newChildRelationship.m_prev == entt::null,
				"New child already has siblings");

			if (m_firstChild == entt::null) // Adding a single node
			{
				SEAssert(m_lastChild == entt::null, "Last child should also be null");

				newChildRelationship.m_next = newChild;
				newChildRelationship.m_prev = newChild;

				m_firstChild = newChild;
				m_lastChild = newChild;
			}
			else
			{
				Relationship& firstChildRelationship = em.GetComponent<fr::Relationship>(m_firstChild);
				Relationship& lastChildRelationship = em.GetComponent<fr::Relationship>(m_lastChild);

				SEAssert(lastChildRelationship.m_next == m_firstChild,
					"Relationship linked list is corrupt: Last node does not point to the first node");

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

			SEAssert(m_firstChild != entt::null && m_lastChild != entt::null,
				"Trying to remove a child from a Relationship that has no children");

			Relationship& childRelationship = em.GetComponent<fr::Relationship>(child);

			bool foundChild = false;

			if (m_firstChild == m_lastChild) // Removing the only node
			{
				SEAssert(m_firstChild == child && m_lastChild == child,
					"Trying to remove an entity that is not a child of the current Relationship");

				m_firstChild = entt::null;
				m_lastChild = entt::null;

				foundChild = true;
			}
			else
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

						// Update the first child marker, if necessary:
						if (currentChild == m_firstChild)
						{
							m_firstChild = nextChildRelationship.m_thisEntity;
						}

						foundChild = true;
						break;
					}
					currentChild = currentChildRelationship.m_next;
				}

				// Handle the last node:
				if (!foundChild)
				{
					SEAssert(child == m_lastChild, "Searched every node but failed to find child");

					Relationship& lastChildRelationship = em.GetComponent<fr::Relationship>(m_lastChild);

					Relationship& prevChildRelationship = 
						em.GetComponent<fr::Relationship>(lastChildRelationship.m_prev);
					SEAssert(lastChildRelationship.m_next == m_firstChild,
						"Last child's next should be the first child");

					Relationship& firstChildRelationship = em.GetComponent<fr::Relationship>(m_firstChild);

					prevChildRelationship.m_next = m_firstChild;
					firstChildRelationship.m_prev = prevChildRelationship.m_thisEntity;

					// Update the last child marker:
					m_lastChild = prevChildRelationship.m_thisEntity;

					foundChild = true;
				}
			}

			// Finally, cleanup the child's linked list references:
			SEAssert(foundChild == true, "Could not find the child to remove");
			childRelationship.m_prev = entt::null;
			childRelationship.m_next = entt::null;
		}
	}
}