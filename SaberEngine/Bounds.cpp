#include <vector>

#include <glm/glm.hpp>

#include "Bounds.h"
#include "Transform.h"


namespace gr
{
	using glm::vec3;
	using glm::vec4;
	using glm::mat4;
	using gr::Transform;

	constexpr float k_bounds3DDepthBias = 0.01f; // Offset to ensure axis min != axis max


	Bounds::Bounds() :
		m_minXYZ(k_invalidMinXYZ),
		m_maxXYZ(k_invalidMaxXYZ) 
	{
	}


	Bounds::Bounds(glm::vec3 minXYZ, glm::vec3 maxXYZ)
		: m_minXYZ(minXYZ)
		, m_maxXYZ(maxXYZ)
	{
	}


	// Returns a new AABB Bounds, transformed from local space using transform
	Bounds Bounds::GetTransformedAABBBounds(mat4 const& worldMatrix)
	{
		// Temp: Ensure the bounds are 3D here, before we do any calculations
		Make3Dimensional();

		Bounds result;

		// Assemble our current AABB points into a cube of 8 vertices:
		std::vector<vec4>points(8);											// "front" == fwd == Z -
		points[0] = vec4(xMin(), yMax(), zMin(), 1.0f);		// Left		top		front 
		points[1] = vec4(xMax(), yMax(), zMin(), 1.0f);		// Right	top		front
		points[2] = vec4(xMin(), yMin(), zMin(), 1.0f);		// Left		bot		front
		points[3] = vec4(xMax(), yMin(), zMin(), 1.0f);		// Right	bot		front

		points[4] = vec4(xMin(), yMax(), zMax(), 1.0f);		// Left		top		back
		points[5] = vec4(xMax(), yMax(), zMax(), 1.0f);		// Right	top		back
		points[6] = vec4(xMin(), yMin(), zMax(), 1.0f);		// Left		bot		back
		points[7] = vec4(xMax(), yMin(), zMax(), 1.0f);		// Right	bot		back

		// Transform each point into world space, and record the min/max coordinate in each dimension:
		for (size_t i = 0; i < 8; i++)
		{
			points[i] = worldMatrix * points[i];

			if (points[i].x < result.xMin())
			{
				result.xMin() = points[i].x;
			}
			if (points[i].x > result.xMax())
			{
				result.xMax() = points[i].x;
			}

			if (points[i].y < result.yMin())
			{
				result.yMin() = points[i].y;
			}
			if (points[i].y > result.yMax())
			{
				result.yMax() = points[i].y;
			}

			if (points[i].z < result.zMin())
			{
				result.zMin() = points[i].z;
			}
			if (points[i].z > result.zMax())
			{
				result.zMax() = points[i].z;
			}
		}

		// Result is an AABB Bounds
		return result;
	}


	void Bounds::ComputeBounds(std::vector<glm::vec3> const& positions)
	{
		for (size_t i = 0; i < positions.size(); i++)
		{
			if (positions[i].x < xMin())
			{
				xMin() = positions[i].x;
			}
			if (positions[i].x > xMax())
			{
				xMax() = positions[i].x;
			}

			if (positions[i].y < yMin())
			{
				yMin() = positions[i].y;
			}
			if (positions[i].y > yMax())
			{
				yMax() = positions[i].y;
			}

			if (positions[i].z < zMin())
			{
				zMin() = positions[i].z;
			}
			if (positions[i].z > zMax())
			{
				zMax() = positions[i].z;
			}
		}
	}


	void Bounds::ExpandBounds(Bounds const& newContents)
	{
		if (newContents.xMin() < xMin())
		{
			xMin() = newContents.xMin();
		}
		if (newContents.xMax() > xMax())
		{
			xMax() = newContents.xMax();
		}

		if (newContents.yMin() < yMin())
		{
			yMin() = newContents.yMin();
		}
		if (newContents.yMax() > yMax())
		{
			yMax() = newContents.yMax();
		}

		if (newContents.zMin() < zMin())
		{
			zMin() = newContents.zMin();
		}
		if (newContents.zMax() > zMax())
		{
			zMax() = newContents.zMax();
		}
	}


	void Bounds::UpdateAABBBounds(Transform* transform)
	{
		*this = GetTransformedAABBBounds(transform->GetGlobalMatrix(Transform::TRS));
	}


	void Bounds::Make3Dimensional()
	{
		if (glm::abs(xMax() - xMin()) < k_bounds3DDepthBias)
		{
			xMax() += k_bounds3DDepthBias;
			xMin() -= k_bounds3DDepthBias;
		}

		if (glm::abs(yMax() - yMin()) < k_bounds3DDepthBias)
		{
			yMax() += k_bounds3DDepthBias;
			yMin() -= k_bounds3DDepthBias;
		}

		if (glm::abs(zMax() - zMin()) < k_bounds3DDepthBias)
		{
			zMax() += k_bounds3DDepthBias;
			zMin() -= k_bounds3DDepthBias;
		}
	}
}