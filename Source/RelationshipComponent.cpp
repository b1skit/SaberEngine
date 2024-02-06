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
		, m_isValid(true)
	{
	}


	Relationship::Relationship(Relationship&& rhs)  noexcept
	{
		*this = std::move(rhs);
	}


	Relationship& Relationship::operator=(Relationship&& rhs) noexcept
	{
		if (this == &rhs)
		{
			return *this;
		}

		{
			std::scoped_lock lock(m_relationshipMutex, rhs.m_relationshipMutex);

			m_thisEntity = rhs.m_thisEntity;
			rhs.m_thisEntity = entt::null;

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
			
			m_isValid = rhs.m_isValid;
			rhs.m_isValid = false;

			return *this;
		}
	}


	Relationship::~Relationship()
	{
		SEAssert(!m_isValid, 
			"Relationship is being destroyed before it is invalidated. Destroy() must be called to remove a "
			"Relationship from its hierarchy");
	}


	void Relationship::Destroy()
	{
		SEAssert(m_isValid, "Trying to destroy a Relationship that is already invalid");
		m_isValid = false;

		// Need to manually destroy relationships; We can't rely on the DTOR as it is only called once the registry has
		// swapped the object out with another
		{
			fr::EntityManager& em = *fr::EntityManager::Get();

			fr::Relationship& relationship = em.GetComponent<fr::Relationship>(m_thisEntity);
			relationship.SetParent(em, entt::null);

			entt::entity curChild = relationship.GetFirstChild();
			while (curChild != entt::null)
			{
				fr::Relationship& childRelationship = em.GetComponent<fr::Relationship>(curChild);
				childRelationship.SetParent(em, entt::null);

				curChild = childRelationship.GetNext();
			}
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
				writeLock.unlock();
				Relationship& prevParentRelationship = em.GetComponent<fr::Relationship>(m_parent);
				writeLock.lock();

				prevParentRelationship.RemoveChild(em, m_thisEntity);
			}

			// Update ourselves:
			m_parent = newParent;

			// Update the parent:
			if (newParent != entt::null)
			{
				writeLock.unlock();
				Relationship& newParentRelationship = em.GetComponent<fr::Relationship>(newParent);
				writeLock.lock();

				newParentRelationship.AddChild(em, m_thisEntity);
			}
		}
	}


	void Relationship::AddChild(EntityManager& em, entt::entity newChild)
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_relationshipMutex);

			// Children are added to the end of our linked list
			writeLock.unlock();
			Relationship& newChildRelationship = em.GetComponent<fr::Relationship>(newChild);
			writeLock.lock();

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
				writeLock.unlock();
				Relationship& firstChildRelationship = em.GetComponent<fr::Relationship>(m_firstChild);
				Relationship& lastChildRelationship = em.GetComponent<fr::Relationship>(m_lastChild);
				writeLock.lock();

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

			writeLock.unlock();
			Relationship& childRelationship = em.GetComponent<fr::Relationship>(child);
			writeLock.lock();

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
					writeLock.unlock();
					Relationship& currentChildRelationship = em.GetComponent<fr::Relationship>(currentChild);
					writeLock.lock();

					if (child == currentChildRelationship.m_thisEntity)
					{
						writeLock.unlock();
						Relationship& prevChildRelationship = 
							em.GetComponent<fr::Relationship>(currentChildRelationship.m_prev);
						Relationship& nextChildRelationship = 
							em.GetComponent<fr::Relationship>(currentChildRelationship.m_next);
						writeLock.lock();

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
					
					writeLock.unlock();
					Relationship& lastChildRelationship = em.GetComponent<fr::Relationship>(m_lastChild);

					Relationship& prevChildRelationship = 
						em.GetComponent<fr::Relationship>(lastChildRelationship.m_prev);
					SEAssert(lastChildRelationship.m_next == m_firstChild,
						"Last child's next should be the first child");

					Relationship& firstChildRelationship = em.GetComponent<fr::Relationship>(m_firstChild);
					writeLock.lock();

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


	std::vector<entt::entity> Relationship::GetAllDescendents() const
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		std::vector<entt::entity> descendents;

		SEAssert((m_firstChild != entt::null && m_lastChild != entt::null) || 
			(m_firstChild == entt::null && m_lastChild == entt::null),
			"Either first and last child must both be null, or both be not null");

		std::stack<entt::entity> remainingChildren;

		// Add the first child:
		if (m_firstChild != entt::null)
		{
			remainingChildren.emplace(m_firstChild);
		}

		while (!remainingChildren.empty())
		{
			const entt::entity current = remainingChildren.top();
			remainingChildren.pop();
			
			// Add the current entity to the output:
			descendents.emplace_back(current);

			// Add any siblings at the same depth the our output:
			fr::Relationship const& currentRelationship = em.GetComponent<fr::Relationship>(current);
			entt::entity sibling = currentRelationship.GetNext();
			while (sibling != current)
			{
				descendents.emplace_back(sibling);

				fr::Relationship const& siblingRelationship = em.GetComponent<fr::Relationship>(sibling);

				// If a sibling has children, add the first one to the stack for the next iteration:
				if (siblingRelationship.m_firstChild != entt::null)
				{
					remainingChildren.push(siblingRelationship.m_firstChild);
				}

				sibling = siblingRelationship.GetNext();
			}

			// Add the first child of the current entity to process the next level:
			const entt::entity firstChild = currentRelationship.GetFirstChild();
			if (firstChild != entt::null)
			{
				remainingChildren.push(firstChild);
			}
		}

		return descendents;
	}
}