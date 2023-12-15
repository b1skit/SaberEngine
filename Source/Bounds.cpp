// © 2022 Adam Badke. All rights reserved.
#include "Bounds.h"
#include "Transform.h"

using glm::vec3;
using glm::vec4;
using glm::mat4;
using gr::Transform;


namespace
{
	constexpr float k_bounds3DDepthBias = 0.01f; // Offset to ensure axis min != axis max
}

namespace gr
{
	Bounds::Bounds()
		: m_minXYZ(k_invalidMinXYZ)
		, m_maxXYZ(k_invalidMaxXYZ) 
	{
	}


	Bounds::Bounds(glm::vec3 minXYZ, glm::vec3 maxXYZ)
		: m_minXYZ(minXYZ)
		, m_maxXYZ(maxXYZ)
	{
	}


	Bounds::Bounds(glm::vec3 minXYZ, glm::vec3 maxXYZ, std::vector<glm::vec3> const& positions)
		: m_minXYZ(minXYZ)
		, m_maxXYZ(maxXYZ)
	{
		if (m_minXYZ == gr::Bounds::k_invalidMinXYZ || m_maxXYZ == gr::Bounds::k_invalidMaxXYZ)
		{
			// Legacy: Previously, we stored vertex data in vecN types. Instead of rewriting, just cast from floats
			ComputeBounds(positions);
		}
	}


	bool Bounds::operator==(gr::Bounds const& rhs) const
	{
		return m_minXYZ == rhs.m_minXYZ && m_maxXYZ == rhs.m_maxXYZ;
	}


	bool Bounds::operator!=(gr::Bounds const& rhs) const
	{
		return operator==(rhs) == false;
	}


	// Returns a new AABB Bounds, transformed from local space using transform
	Bounds Bounds::GetTransformedAABBBounds(mat4 const& worldMatrix) const
	{
		// Assemble our current AABB points into a cube of 8 vertices:
		std::vector<vec4>points(8);							// "front" == fwd == Z -
		points[0] = vec4(xMin(), yMax(), zMin(), 1.0f);		// Left		top		front 
		points[1] = vec4(xMax(), yMax(), zMin(), 1.0f);		// Right	top		front
		points[2] = vec4(xMin(), yMin(), zMin(), 1.0f);		// Left		bot		front
		points[3] = vec4(xMax(), yMin(), zMin(), 1.0f);		// Right	bot		
		points[4] = vec4(xMin(), yMax(), zMax(), 1.0f);		// Left		top		back
		points[5] = vec4(xMax(), yMax(), zMax(), 1.0f);		// Right	top		back
		points[6] = vec4(xMin(), yMin(), zMax(), 1.0f);		// Left		bot		back
		points[7] = vec4(xMax(), yMin(), zMax(), 1.0f);		// Right	bot		back

		// Compute a new AABB in world-space:
		Bounds result; // Invalid min/max by default

		// Transform each point into world space, and record the min/max coordinate in each dimension:
		for (size_t i = 0; i < 8; i++)
		{
			points[i] = worldMatrix * points[i];

			result.m_minXYZ.x = std::min(points[i].x, result.m_minXYZ.x );
			result.m_maxXYZ.x = std::max(points[i].x, result.m_maxXYZ.x);

			result.m_minXYZ.y = std::min(points[i].y, result.m_minXYZ.y);
			result.m_maxXYZ.y = std::max(points[i].y, result.m_maxXYZ.y);

			result.m_minXYZ.z = std::min(points[i].z, result.m_minXYZ.z);
			result.m_maxXYZ.z = std::max(points[i].z, result.m_maxXYZ.z);
		}

		result.Make3Dimensional(); // Ensure the final bounds are 3D

		return result;
	}


	void Bounds::ComputeBounds(std::vector<glm::vec3> const& positions)
	{
		for (size_t i = 0; i < positions.size(); i++)
		{
			m_minXYZ.x = std::min(positions[i].x, m_minXYZ.x);
			m_maxXYZ.x = std::max(positions[i].x, m_maxXYZ.x);

			m_minXYZ.y = std::min(positions[i].y, m_minXYZ.y);
			m_maxXYZ.y = std::max(positions[i].y, m_maxXYZ.y);

			m_minXYZ.z = std::min(positions[i].z, m_minXYZ.z);
			m_maxXYZ.z = std::max(positions[i].z, m_maxXYZ.z);
		}
	}


	void Bounds::ExpandBounds(Bounds const& newContents)
	{
		m_minXYZ.x = std::min(newContents.m_minXYZ.x, m_minXYZ.x);
		m_maxXYZ.x = std::max(newContents.m_maxXYZ.x, m_maxXYZ.x);

		m_minXYZ.y = std::min(newContents.m_minXYZ.y, m_minXYZ.y);
		m_maxXYZ.y = std::max(newContents.m_maxXYZ.y, m_maxXYZ.y);
		
		m_minXYZ.z = std::min(newContents.m_minXYZ.z, m_minXYZ.z);
		m_maxXYZ.z = std::max(newContents.m_maxXYZ.z, m_maxXYZ.z);
	}


	void Bounds::Make3Dimensional()
	{
		if (glm::abs(m_maxXYZ.x - m_minXYZ.x) < k_bounds3DDepthBias)
		{
			m_minXYZ.x -= k_bounds3DDepthBias;
			m_maxXYZ.x += k_bounds3DDepthBias;
		}

		if (glm::abs(m_maxXYZ.y - m_minXYZ.y) < k_bounds3DDepthBias)
		{
			m_minXYZ.y -= k_bounds3DDepthBias;
			m_maxXYZ.y += k_bounds3DDepthBias;
		}

		if (glm::abs(m_maxXYZ.z - m_minXYZ.z) < k_bounds3DDepthBias)
		{
			m_minXYZ.z -= k_bounds3DDepthBias;
			m_maxXYZ.z += k_bounds3DDepthBias;
		}
	}


	void Bounds::ShowImGuiWindow() const
	{
		ImGui::Text("Min XYZ = %s", glm::to_string(m_minXYZ).c_str());
		ImGui::Text("Max XYZ = %s", glm::to_string(m_maxXYZ).c_str());
	}
}