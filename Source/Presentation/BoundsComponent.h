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


	class BoundsComponent
	{
	public:
		static constexpr glm::vec3 k_invalidMinXYZ = glm::vec3(std::numeric_limits<float>::max());
		static constexpr glm::vec3 k_invalidMaxXYZ = -glm::vec3(std::numeric_limits<float>::max());
		// Note: -max is the furthest away from max

	public:
		struct SceneBoundsMarker {}; // Unique: Only added to 1 bounds component for the entire scene


	public:
		static void CreateSceneBoundsConcept(fr::EntityManager&);
		
		static void AttachBoundsComponent(fr::EntityManager&, entt::entity);

		static void AttachBoundsComponent(
			fr::EntityManager&, 
			entt::entity, 
			glm::vec3 const& minXYZ, 
			glm::vec3 const& maxXYZ);


	public:
		static gr::Bounds::RenderData CreateRenderData(entt::entity, fr::BoundsComponent const&);

		static void ShowImGuiWindow(fr::EntityManager&, entt::entity owningEntity, bool startOpen = false);


	public:
		static BoundsComponent Zero() { return BoundsComponent(PrivateCTORTag{}, glm::vec3(0.f), glm::vec3(0.f)); }
		static BoundsComponent Invalid() { return BoundsComponent(PrivateCTORTag{}); }


		// Returns a new AABB BoundsConcept, transformed from local -> global space using the given matrix
		BoundsComponent GetTransformedAABBBounds(glm::mat4 const& worldMatrix) const;

		// Expands a bounds to contain another Bounds
		void ExpandBounds(BoundsComponent const& newContents);

		// Recursively expand the current Bounds, and any Bounds found in the Relationship hierarchy above
		void ExpandBoundsHierarchy(fr::EntityManager&, BoundsComponent const& newContents, entt::entity boundsEntity);

		float xMin() const;		
		float xMax() const;
		float yMin() const;
		float yMax() const;				  
		float zMin() const;
		float zMax() const;

		void SetEncapsulatingBoundsRenderDataID(gr::RenderDataID);
		gr::RenderDataID GetEncapsulatingBoundsRenderDataID() const;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		BoundsComponent(PrivateCTORTag);
		explicit BoundsComponent(PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ);

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


	inline void BoundsComponent::SetEncapsulatingBoundsRenderDataID(gr::RenderDataID renderDataID)
	{
		m_encapsulatingBoundsRenderDataID = renderDataID;
	}


	inline gr::RenderDataID BoundsComponent::GetEncapsulatingBoundsRenderDataID() const
	{
		return m_encapsulatingBoundsRenderDataID;
	}
}