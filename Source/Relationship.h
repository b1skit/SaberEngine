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


	private:
		void AddChild(GameplayManager&, entt::entity);
		void RemoveChild(GameplayManager&, entt::entity);
		

	public: // Use AttachRelationshipComponent instead
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
	};


	inline entt::entity Relationship::GetParent() const
	{
		return m_parent;
	}


	inline entt::entity Relationship::GetNext() const
	{
		return m_next;
	}


	inline entt::entity Relationship::GetPrev() const
	{
		return m_prev;
	}


	inline entt::entity Relationship::GetFirstChild() const
	{
		return m_firstChild;
	}


	inline entt::entity Relationship::GetLastChild() const
	{
		return m_lastChild;
	}
}