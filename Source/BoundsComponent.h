// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "BoundsRenderData.h"


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
		enum class Contents
		{
			Mesh,
			MeshPrimitive,
			Scene, // Unique: Only added to 1 bounds component for the entire scene
		};

		struct MeshBoundsMarker {};
		struct MeshPrimitiveBoundsMarker {};
		struct SceneBoundsMarker {}; // Unique: Only added to 1 bounds component for the entire scene


	public:
		static void CreateSceneBoundsConcept(fr::EntityManager&);
		
		static void AttachBoundsComponent(fr::EntityManager&, entt::entity, BoundsComponent::Contents);

		static void AttachBoundsComponent(
			fr::EntityManager&, 
			entt::entity, 
			glm::vec3 const& minXYZ, 
			glm::vec3 const& maxXYZ,
			BoundsComponent::Contents);

		static void AttachBoundsComponent(
			fr::EntityManager&, 
			entt::entity, 
			glm::vec3 const& minXYZ, 
			glm::vec3 const& maxXYZ, 
			std::vector<glm::vec3> const& positions,
			BoundsComponent::Contents);

	public:
		static gr::Bounds::RenderData CreateRenderData(fr::BoundsComponent const&, fr::NameComponent const&);


	private:
		static void AttachMarkers(fr::EntityManager&, entt::entity, Contents);


	public:
		static BoundsComponent Uninitialized() { return BoundsComponent(); }
		static BoundsComponent Zero() { return BoundsComponent(PrivateCTORTag{}, glm::vec3(0.f), glm::vec3(0.f)); }


		// Returns a new AABB BoundsConcept, transformed from local space using transform
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

		void ShowImGuiWindow() const;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		BoundsComponent(PrivateCTORTag);
		explicit BoundsComponent(PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ);
		explicit BoundsComponent(PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ, std::vector<glm::vec3> const& positions);

		BoundsComponent(BoundsComponent const& rhs) = default;
		BoundsComponent(BoundsComponent&&) = default;
		BoundsComponent& operator=(BoundsComponent const& rhs) = default;
		~BoundsComponent() = default;

		bool operator==(fr::BoundsComponent const&) const;
		bool operator!=(fr::BoundsComponent const&) const;


	private:
		void ComputeBounds(std::vector<glm::vec3> const& positions);
		void Make3Dimensional();


	private: // Axis-Aligned Bounding Box (AABB) points
		glm::vec3 m_minXYZ;
		glm::vec3 m_maxXYZ;


	private:
		BoundsComponent();
	};


	inline float BoundsComponent::xMin() const 
	{ 
		return m_minXYZ.x;
	}


	inline float BoundsComponent::xMax() const 
	{ 
		return m_maxXYZ.x;
	}


	inline float BoundsComponent::yMin() const 
	{ 
		return m_minXYZ.y;
	}


	inline float BoundsComponent::yMax() const 
	{ 
		return m_maxXYZ.y;
	}


	inline float BoundsComponent::zMin() const 
	{ 
		return m_minXYZ.z;
	}


	inline float BoundsComponent::zMax() const 
	{ 
		return m_maxXYZ.z;
	}
}