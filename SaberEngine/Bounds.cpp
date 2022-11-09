#include <vector>

#include <glm/glm.hpp>

#include "Bounds.h"

namespace re
{
	using glm::mat4;
	using glm::vec4;

	constexpr float k_bounds3DDepthBias = 0.01f; // Offset to ensure axis min != axis max


	Bounds::Bounds() :
		m_minXYZ(glm::vec3(std::numeric_limits<float>::max())),
		m_maxXYZ(-glm::vec3(std::numeric_limits<float>::max())) // Note: -max is the furthest away from max
	{
	}


	// Returns a Bounds, transformed from local space using worldMatrix
	Bounds Bounds::GetTransformedBounds(mat4 const& worldMatrix)
	{
		// Temp: Ensure the bounds are 3D here, before we do any calculations
		Make3Dimensional();

		Bounds result;

		std::vector<vec4>points(8);											// "front" == fwd == Z -
		points[0] = vec4(xMin(), yMax(), zMin(), 1.0f);		// Left		top		front 
		points[1] = vec4(xMax(), yMax(), zMin(), 1.0f);		// Right	top		front
		points[2] = vec4(xMin(), yMin(), zMin(), 1.0f);		// Left		bot		front
		points[3] = vec4(xMax(), yMin(), zMin(), 1.0f);		// Right	bot		front

		points[4] = vec4(xMin(), yMax(), zMax(), 1.0f);		// Left		top		back
		points[5] = vec4(xMax(), yMax(), zMax(), 1.0f);		// Right	top		back
		points[6] = vec4(xMin(), yMin(), zMax(), 1.0f);		// Left		bot		back
		points[7] = vec4(xMax(), yMin(), zMax(), 1.0f);		// Right	bot		back

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

		return result;
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