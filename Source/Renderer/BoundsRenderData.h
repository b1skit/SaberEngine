// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"

#include "Core/Assert.h"


namespace gr
{
	class Bounds
	{
	public:
		struct RenderData final
		{
			gr::RenderDataID m_encapsulatingBounds = gr::k_invalidRenderDataID;

			// Axis-Aligned Bounding Box (AABB) points
			glm::vec3 m_localMinXYZ;
			glm::vec3 m_localMaxXYZ;

			glm::vec3 m_worldMinXYZ;
			glm::vec3 m_worldMaxXYZ;
		};


		static void ComputeMinMaxPosition(std::span<const glm::vec3> positions, glm::vec3* minXYZ, glm::vec3* maxXYZ)
		{
			if (!minXYZ && !maxXYZ)
			{
				return;
			}
			SEAssert(!positions.empty(), "Cannot compute min/max from an empty positions vector");

			glm::vec3 minResult(std::numeric_limits<float>::max());
			glm::vec3 maxResult(std::numeric_limits<float>::min());
			for (const auto& position : positions)
			{
				minResult.x = std::min(position.x, minResult.x);
				minResult.y = std::min(position.y, minResult.y);
				minResult.z = std::min(position.z, minResult.z);

				maxResult.x = std::max(position.x, maxResult.x);
				maxResult.y = std::max(position.y, maxResult.y);
				maxResult.z = std::max(position.z, maxResult.z);
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

		// Legacy overload for compatibility
		static void ComputeMinMaxPosition(glm::vec3 const* positions, size_t numPositions, glm::vec3* minXYZ, glm::vec3* maxXYZ)
		{
			ComputeMinMaxPosition(std::span<const glm::vec3>{positions, numPositions}, minXYZ, maxXYZ);
		}
	};
}