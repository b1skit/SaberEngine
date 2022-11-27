#pragma once

namespace re
{
	class Bounds
	{
	public:
		static constexpr glm::vec3 k_invalidMinXYZ = glm::vec3(std::numeric_limits<float>::max());
		static constexpr glm::vec3 k_invalidMaxXYZ = -glm::vec3(std::numeric_limits<float>::max());
		// Note: -max is the furthest away from max

	public:
		Bounds();
		explicit Bounds(glm::vec3 minXYZ, glm::vec3 maxXYZ);

		Bounds(Bounds const& rhs) = default;
		Bounds(Bounds&&) = default;
		Bounds& operator=(Bounds const& rhs) = default;
		~Bounds() = default;

		// Returns a new AABB Bounds, transformed from local space using transform
		Bounds GetTransformedBounds(glm::mat4 const& worldMatrix);

		void ComputeBounds(std::vector<glm::vec3> const& positions);

		void ExpandBounds(Bounds const& newContents); // Expands a bounds to contain another bounds

		inline float xMin() const { return m_minXYZ.x; }		
		inline float xMax() const { return m_maxXYZ.x; }

		inline float yMin() const { return m_minXYZ.y; }		
		inline float yMax() const { return m_maxXYZ.y; }
		
		inline float zMin() const { return m_minXYZ.z; }
		inline float zMax() const { return m_maxXYZ.z; }


	private:
		void Make3Dimensional();

		inline float& xMin() { return m_minXYZ.x; }
		inline float& xMax() { return m_maxXYZ.x; }

		inline float& yMin() { return m_minXYZ.y; }
		inline float& yMax() { return m_maxXYZ.y; }

		inline float& zMin() { return m_minXYZ.z; }
		inline float& zMax() { return m_maxXYZ.z; }

	private:
		// Axis-Aligned Bounding Box (AABB) points:
		glm::vec3 m_minXYZ;
		glm::vec3 m_maxXYZ;
	};
}