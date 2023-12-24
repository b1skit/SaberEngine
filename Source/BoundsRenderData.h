// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace gr
{
	class Bounds
	{
	public:
		struct RenderData
		{
			// Axis-Aligned Bounding Box (AABB) points
			glm::vec3 m_minXYZ;
			glm::vec3 m_maxXYZ;
		};
	};
}