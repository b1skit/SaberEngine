// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Bounds.h"


namespace fr
{
	class GameplayManager;


	class Bounds
	{
	public:
		static constexpr glm::vec3 k_invalidMinXYZ = glm::vec3(std::numeric_limits<float>::max());
		static constexpr glm::vec3 k_invalidMaxXYZ = -glm::vec3(std::numeric_limits<float>::max());
		// Note: -max is the furthest away from max


	public:
		struct IsSceneBoundsMarker {}; //Unique: Only added to 1 bounds component for the entire scene

		static gr::Bounds::RenderData CreateRenderData(fr::Bounds const&);


	public:
		static void CreateSceneBounds(fr::GameplayManager&);
		
		static void AttachBoundsComponent(fr::GameplayManager&, entt::entity);

		static void AttachBoundsComponent(
			fr::GameplayManager&, 
			entt::entity, 
			glm::vec3 const& minXYZ, 
			glm::vec3 const& maxXYZ);

		static void AttachBoundsComponent(
			fr::GameplayManager&, 
			entt::entity, 
			glm::vec3 const& minXYZ, 
			glm::vec3 const& maxXYZ, 
			std::vector<glm::vec3> const& positions);

	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		Bounds(PrivateCTORTag);
		explicit Bounds(PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ);
		explicit Bounds(PrivateCTORTag, glm::vec3 const& minXYZ, glm::vec3 const& maxXYZ, std::vector<glm::vec3> const& positions);

		Bounds(Bounds const& rhs) = default;
		Bounds(Bounds&&) = default;
		Bounds& operator=(Bounds const& rhs) = default;
		~Bounds() = default;

		bool operator==(fr::Bounds const&) const;
		bool operator!=(fr::Bounds const&) const;


	public:
		static Bounds Uninitialized() { return Bounds(); }


		// Returns a new AABB BoundsConcept, transformed from local space using transform
		Bounds GetTransformedAABBBounds(glm::mat4 const& worldMatrix) const;

		void ExpandBounds(Bounds const& newContents); // Expands a bounds to contain another bounds

		float xMin() const;		
		float xMax() const;
		float yMin() const;
		float yMax() const;				  
		float zMin() const;
		float zMax() const;


		void ShowImGuiWindow() const;


	private:
		void ComputeBounds(std::vector<glm::vec3> const& positions);
		void Make3Dimensional();


	private: // Axis-Aligned Bounding Box (AABB) points
		glm::vec3 m_minXYZ;
		glm::vec3 m_maxXYZ;


	private:
		Bounds();
	};


	inline float Bounds::xMin() const 
	{ 
		return m_minXYZ.x;
	}


	inline float Bounds::xMax() const 
	{ 
		return m_maxXYZ.x;
	}


	inline float Bounds::yMin() const 
	{ 
		return m_minXYZ.y;
	}


	inline float Bounds::yMax() const 
	{ 
		return m_maxXYZ.y;
	}


	inline float Bounds::zMin() const 
	{ 
		return m_minXYZ.z;
	}


	inline float Bounds::zMax() const 
	{ 
		return m_maxXYZ.z;
	}
}