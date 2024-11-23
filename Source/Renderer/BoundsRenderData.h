// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"

#include "Core/Assert.h"


namespace gr
{
	class Bounds
	{
	public:
		struct RenderData
		{
			gr::RenderDataID m_encapsulatingBounds = gr::k_invalidRenderDataID;

			// Axis-Aligned Bounding Box (AABB) points
			glm::vec3 m_localMinXYZ;
			glm::vec3 m_localMaxXYZ;

			glm::vec3 m_worldMinXYZ;
			glm::vec3 m_worldMaxXYZ;
		};


		static void ComputeMinMaxPosition(glm::vec3 const* positions, size_t numPositions, glm::vec3* minXYZ, glm::vec3* maxXYZ)
		{
			if (!minXYZ && !maxXYZ)
			{
				return;
			}
			SEAssert(positions && numPositions > 0, "Cannot compute min/max from an empty positions vector");

			glm::vec3 minResult(std::numeric_limits<float>::max());
			glm::vec3 maxResult(std::numeric_limits<float>::min());
			for (size_t i = 0; i < numPositions; ++i)
			{
				minResult.x = std::min(positions[i].x, minResult.x);
				minResult.y = std::min(positions[i].y, minResult.y);
				minResult.z = std::min(positions[i].z, minResult.z);

				maxResult.x = std::max(positions[i].x, maxResult.x);
				maxResult.y = std::max(positions[i].y, maxResult.y);
				maxResult.z = std::max(positions[i].z, maxResult.z);
			}

			if (minXYZ)
			{
				*minXYZ = minResult;
			}
			if (maxXYZ)
			{
				*maxXYZ = maxResult;
			}
		}
	};
}