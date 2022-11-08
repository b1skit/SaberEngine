#pragma once

namespace gr
{
	class Bounds
	{
	public:
		Bounds();

		Bounds(Bounds const& rhs) = default;
		Bounds(Bounds&&) = default;
		Bounds& operator=(Bounds const& rhs) = default;
		~Bounds() = default;

		inline float& xMin() { return m_minXYZ.x; }
		inline float xMin() const { return m_minXYZ.x; }

		inline float& xMax() { return m_maxXYZ.x; }
		inline float xMax() const { return m_maxXYZ.x; }

		inline float& yMin() { return m_minXYZ.y; }
		inline float yMin() const { return m_minXYZ.y; }

		inline float& yMax() { return m_maxXYZ.y; }
		inline float yMax() const { return m_maxXYZ.y; }

		inline float& zMin() { return m_minXYZ.z; }
		inline float zMin() const { return m_minXYZ.z; }

		inline float& zMax() { return m_maxXYZ.z; }
		inline float zMax() const { return m_maxXYZ.z; }

		// Returns a Bounds, transformed from local space using transform
		Bounds GetTransformedBounds(glm::mat4 const& m_transform);

		void Make3Dimensional();

	private:
		glm::vec3 m_minXYZ;
		glm::vec3 m_maxXYZ;
	};
}