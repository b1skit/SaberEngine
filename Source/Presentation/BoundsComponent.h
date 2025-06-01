// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/BoundsRenderData.h"


namespace util
{
	class ByteVector;
}

namespace fr
{
	class EntityManager;
	class NameComponent;
	class Relationship;


	class BoundsComponent final
	{
	public:
		static constexpr glm::vec3 k_invalidMinXYZ = glm::vec3(std::numeric_limits<float>::max());
		static constexpr glm::vec3 k_invalidMaxXYZ = -glm::vec3(std::numeric_limits<float>::max());
		// Note: -max is the furthest away from max

	public:
		struct SceneBoundsMarker final {}; // Unique: Only added to 1 bounds component for the entire scene


	public:
		static fr::BoundsComponent& CreateSceneBoundsConcept(fr::EntityManager&);
		
		static fr::BoundsComponent& AttachBoundsComponent(fr::EntityManager&, entt::entity owningEntity, entt::entity encapsulatingBounds);

		static fr::BoundsComponent& AttachBoundsComponent(
			fr::EntityManager&, 
			entt::entity,
			entt::entity encapsulatingBounds,
			glm::vec3 const& minXYZ, 
			glm::vec3 const& maxXYZ);

		static void UpdateBoundsComponent(
			fr::EntityManager&, fr::BoundsComponent&, fr::Relationship const&, entt::entity);


	public:
		static gr::Bounds::RenderData CreateRenderData(entt::entity, fr::BoundsComponent const&);

		static void ShowImGuiWindow(fr::EntityManager&, entt::entity owningEntity, bool startOpen = false);


	public:
		static BoundsComponent Zero() { return BoundsComponent(PrivateCTORTag{}, glm::vec3(0.f), glm::vec3(0.f), entt::null); }
		static BoundsComponent Invalid() { return BoundsComponent(PrivateCTORTag{}); }

		static void MarkDirty(entt::entity boundsEntity);

		// Returns a new AABB BoundsConcept, transformed from local -> global space using the given matrix
		BoundsComponent GetTransformedAABBBounds(glm::mat4 const& worldMatrix) const;

		// Expands a Bounds to contain another Bounds
		void ExpandBounds(BoundsComponent const& newContents, entt::entity boundsEntity);
		void ExpandBounds(glm::vec3 const& newLocalMinXYZ, glm::vec3 const& newLocalMaxXYZ, entt::entity boundsEntity);

		float xMin() const;		
		float xMax() const;
		float yMin() const;
		float yMax() const;				  
		float zMin() const;
		float zMax() const;

		glm::vec3 const& GetOriginalMinXYZ() const; // Min/Max XYZ at creation (e.g. For updating for skinned bounds)
		glm::vec3 const& GetOriginalMaxXYZ() const;

		glm::vec3 const& GetLocalMinXYZ() const;
		glm::vec3 const& GetLocalMaxXYZ() const;

		void SetLocalMinXYZ(glm::vec3 const&, entt::entity boundsEntity);
		void SetLocalMaxXYZ(glm::vec3 const&, entt::entity boundsEntity);
		void SetLocalMinMaxXYZ(glm::vec3 const&, glm::vec3 const&, entt::entity boundsEntity);

		void SetEncapsulatingBounds(entt::entity, gr::RenderDataID);
		entt::entity GetEncapsulatingBoundsEntity() const;
		gr::RenderDataID GetEncapsulatingBoundsRenderDataID() const;


	private:

		void ExpandEncapsulatingBounds(
			fr::EntityManager&, BoundsComponent const& newContents, entt::entity boundsEntity);
		void ExpandEncapsulatingBounds(
			fr::EntityManager&, glm::vec3 const& newLocalMinXYZ, glm::vec3 const& newLocalMaxXYZ, entt::entity boundsEntity);

		// Returns true if the bounds was modified, false otherwise
		bool ExpandBoundsInternal(
			glm::vec3 const& newMinXYZ, glm::vec3 const& newMaxXYZ, entt::entity boundsEntity);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		BoundsComponent(PrivateCTORTag);
		explicit BoundsComponent(
			PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ, entt::entity encapsulatingBounds);

		BoundsComponent(BoundsComponent const& rhs) = default;
		BoundsComponent(BoundsComponent&&) noexcept = default;
		BoundsComponent& operator=(BoundsComponent const&) = default;
		BoundsComponent& operator=(BoundsComponent&&) noexcept = default;
		~BoundsComponent() = default;

		bool operator==(fr::BoundsComponent const&) const;
		bool operator!=(fr::BoundsComponent const&) const;


	private:
		void Make3Dimensional();


	private: // Axis-Aligned Bounding Box (AABB) points
		glm::vec3 m_localMinXYZ;
		glm::vec3 m_localMaxXYZ;

		// AABB at creation: Used when updating for skinning
		glm::vec3 m_originalMinXYZ; 
		glm::vec3 m_originalMaxXYZ;

		entt::entity m_encapsulatingBoundsEntity;
		gr::RenderDataID m_encapsulatingBoundsRenderDataID;

	private:
		BoundsComponent() = delete;
	};


	inline float BoundsComponent::xMin() const 
	{ 
		return m_localMinXYZ.x;
	}


	inline float BoundsComponent::xMax() const 
	{ 
		return m_localMaxXYZ.x;
	}


	inline float BoundsComponent::yMin() const 
	{ 
		return m_localMinXYZ.y;
	}


	inline float BoundsComponent::yMax() const 
	{ 
		return m_localMaxXYZ.y;
	}


	inline float BoundsComponent::zMin() const 
	{ 
		return m_localMinXYZ.z;
	}


	inline float BoundsComponent::zMax() const 
	{ 
		return m_localMaxXYZ.z;
	}


	inline glm::vec3 const& BoundsComponent::GetOriginalMinXYZ() const
	{
		return m_originalMinXYZ;
	}


	inline glm::vec3 const& BoundsComponent::GetOriginalMaxXYZ() const
	{
		return m_originalMaxXYZ;
	}


	inline glm::vec3 const& BoundsComponent::GetLocalMinXYZ() const
	{
		return m_localMinXYZ;
	}


	inline glm::vec3 const& BoundsComponent::GetLocalMaxXYZ() const
	{
		return m_localMaxXYZ;
	}


	inline void BoundsComponent::SetEncapsulatingBounds(entt::entity entity, gr::RenderDataID renderDataID)
	{
		m_encapsulatingBoundsEntity = entity;
		m_encapsulatingBoundsRenderDataID = renderDataID;
	}


	inline entt::entity BoundsComponent::GetEncapsulatingBoundsEntity() const
	{
		return m_encapsulatingBoundsEntity;
	}


	inline gr::RenderDataID BoundsComponent::GetEncapsulatingBoundsRenderDataID() const
	{
		return m_encapsulatingBoundsRenderDataID;
	}
}