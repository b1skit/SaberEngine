// © 2023 Adam Badke. All rights reserved.
#pragma once


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
}