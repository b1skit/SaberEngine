// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "RelationshipComponent.h"


namespace pr
{
	Relationship& Relationship::AttachRelationshipComponent(EntityManager& em, entt::entity owningEntity)
	{
		return *em.EmplaceComponent<pr::Relationship>(owningEntity, PrivateCTORTag{}, owningEntity);
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


	void Relationship::Destroy(pr::EntityManager& em)
	{
		// Need to manually destroy relationships; We can't rely on the DTOR as it is only called once the registry has
		// swapped the object out with another

		SEAssert(m_isValid, "Trying to destroy a Relationship that is already invalid");
		m_isValid = false;

		SetParent(em, entt::null);

		// Tell the children to remove themselves:
		entt::entity curChild = GetFirstChild();
		while (curChild != entt::null)
		{
			pr::Relationship& childRelationship = em.GetComponent<pr::Relationship>(curChild);

			// Cache the next child entity before we remove the current one
			const entt::entity nextChild = childRelationship.GetNext();

			childRelationship.SetParent(em, entt::null);

			curChild = nextChild;
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
				Relationship& prevParentRelationship = em.GetComponent<pr::Relationship>(m_parent);

				prevParentRelationship.RemoveChild(em, m_thisEntity);
			}

			// Update ourselves:
			m_parent = newParent;

			// Update the parent:
			if (newParent != entt::null)
			{
				Relationship& newParentRelationship = em.GetComponent<pr::Relationship>(newParent);

				newParentRelationship.AddChild(em, m_thisEntity);
			}
		}
	}


	void Relationship::AddChild(EntityManager& em, entt::entity newChild)
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_relationshipMutex);

			// Children are added to the end of our linked list
			Relationship& newChildRelationship = em.GetComponent<pr::Relationship>(newChild);

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
				Relationship& firstChildRelationship = em.GetComponent<pr::Relationship>(m_firstChild);
				Relationship& lastChildRelationship = em.GetComponent<pr::Relationship>(m_lastChild);

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

			Relationship& childRelationship = em.GetComponent<pr::Relationship>(child);

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
					Relationship& currentChildRelationship = em.GetComponent<pr::Relationship>(currentChild);

					if (child == currentChildRelationship.m_thisEntity)
					{
						Relationship& prevChildRelationship = 
							em.GetComponent<pr::Relationship>(currentChildRelationship.m_prev);
						Relationship& nextChildRelationship = 
							em.GetComponent<pr::Relationship>(currentChildRelationship.m_next);

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
					
					Relationship& lastChildRelationship = em.GetComponent<pr::Relationship>(m_lastChild);

					Relationship& prevChildRelationship = 
						em.GetComponent<pr::Relationship>(lastChildRelationship.m_prev);
					SEAssert(lastChildRelationship.m_next == m_firstChild,
						"Last child's next should be the first child");

					Relationship& firstChildRelationship = em.GetComponent<pr::Relationship>(m_firstChild);

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


	std::vector<entt::entity> Relationship::GetAllDescendents(EntityManager& em) const
	{
		SEAssert((m_firstChild != entt::null && m_lastChild != entt::null) || 
			(m_firstChild == entt::null && m_lastChild == entt::null),
			"Either first and last child must both be null, or both be not null");

		std::vector<entt::entity> descendents;
		{
			std::shared_lock<std::shared_mutex> readLock(m_relationshipMutex);

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
				pr::Relationship const& currentRelationship = em.GetComponent<pr::Relationship>(current);
				entt::entity sibling = currentRelationship.GetNext();
				while (sibling != current)
				{
					descendents.emplace_back(sibling);

					pr::Relationship const& siblingRelationship = em.GetComponent<pr::Relationship>(sibling);

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
		}

		return descendents;
	}


	std::vector<entt::entity> Relationship::GetAllChildren(EntityManager& em) const
	{
		std::vector<entt::entity> siblings;
		{
			std::shared_lock<std::shared_mutex> readLock(m_relationshipMutex);

			if (m_firstChild != entt::null)
			{
				entt::entity cur = m_firstChild;
				while (cur != m_lastChild)
				{
					siblings.emplace_back(cur);

					pr::Relationship const& siblingRelationship = em.GetComponent<pr::Relationship>(cur);

					cur = siblingRelationship.GetNext();
				}
				siblings.emplace_back(m_lastChild);
			}
		}
		return siblings;
	}
}