// © 2023 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "Relationship.h"


namespace fr
{
	Relationship& Relationship::AttachRelationshipComponent(GameplayManager& gpm, entt::entity owningEntity)
	{
		return *gpm.EmplaceComponent<fr::Relationship>(owningEntity, owningEntity);
	}


	Relationship::Relationship(entt::entity owningEntity)
		: m_thisEntity(owningEntity)
		, m_parent(entt::null)
		, m_prev(entt::null)
		, m_next(entt::null)
		, m_firstChild(entt::null)
		, m_lastChild(entt::null)
	{
	}


	void Relationship::SetParent(GameplayManager& gpm, entt::entity newParent)
	{
		if (m_parent != entt::null)
		{
			Relationship& prevParentRelationship = gpm.GetComponent<fr::Relationship>(m_parent);

			prevParentRelationship.RemoveChild(gpm, m_thisEntity);
		}

		// Update ourselves:
		m_parent = newParent;
		
		// Update the parent:
		if (newParent != entt::null)
		{
			Relationship& newParentRelationship = gpm.GetComponent<fr::Relationship>(newParent);
			newParentRelationship.AddChild(gpm, m_thisEntity);
		}
	}


	void Relationship::AddChild(GameplayManager& gpm, entt::entity newChild)
	{
		// Children are added to the end of our linked list

		Relationship& newChildRelationship = gpm.GetComponent<fr::Relationship>(newChild);

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
			Relationship& firstChildRelationship = gpm.GetComponent<fr::Relationship>(m_firstChild);
			Relationship& lastChildRelationship = gpm.GetComponent<fr::Relationship>(m_lastChild);

			SEAssert("Relationship linked list is corrupt: Last node does not point to the first node",
				lastChildRelationship.m_next == m_firstChild);

			lastChildRelationship.m_next = newChild;

			newChildRelationship.m_prev = lastChildRelationship.m_thisEntity;
			newChildRelationship.m_next = m_firstChild;

			firstChildRelationship.m_prev = newChild;

			m_lastChild = newChild;
		}	
	}


	void Relationship::RemoveChild(GameplayManager& gpm, entt::entity child)
	{
		SEAssert("Trying to remove a child from a Relationship that has no children", 
			m_firstChild != entt::null && m_lastChild != entt::null);

		Relationship& childRelationship = gpm.GetComponent<fr::Relationship>(child);

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
				Relationship& currentChildRelationship = gpm.GetComponent<fr::Relationship>(currentChild);

				if (child == currentChildRelationship.m_thisEntity)
				{
					Relationship& prevChildRelationship = gpm.GetComponent<fr::Relationship>(currentChildRelationship.m_prev);
					Relationship& nextChildRelationship = gpm.GetComponent<fr::Relationship>(currentChildRelationship.m_next);

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
				Relationship& lastChildRelationship = gpm.GetComponent<fr::Relationship>(m_lastChild);

				Relationship& prevChildRelationship = gpm.GetComponent<fr::Relationship>(lastChildRelationship.m_prev);
				SEAssert("Last child's next should be the first child", lastChildRelationship.m_next == m_firstChild);

				Relationship& firstChildRelationship = gpm.GetComponent<fr::Relationship>(m_firstChild);

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