// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace gr
{
	class Transform;
}

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

		struct RenderData
		{
			// Axis-Aligned Bounding Box (AABB) points
			glm::vec3 m_minXYZ;
			glm::vec3 m_maxXYZ;
		};


	public:
		static void CreateSceneBounds(fr::GameplayManager&);
		static void AttachBoundsComponent(fr::GameplayManager&, entt::entity);
		static RenderData CreateRenderData(fr::Bounds const&);


	public:
		Bounds();
		explicit Bounds(glm::vec3 minXYZ, glm::vec3 maxXYZ);
		explicit Bounds(glm::vec3 minXYZ, glm::vec3 maxXYZ, std::vector<glm::vec3> const& positions);

		Bounds(Bounds const& rhs) = default;
		Bounds(Bounds&&) = default;
		Bounds& operator=(Bounds const& rhs) = default;
		~Bounds() = default;

		bool operator==(fr::Bounds const&) const;
		bool operator!=(fr::Bounds const&) const;

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