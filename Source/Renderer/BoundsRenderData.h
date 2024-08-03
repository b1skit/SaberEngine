// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"


namespace gr
{
    class Bounds
    {
    public:
        struct RenderData
        {
            gr::RenderDataID m_encapsulatingBounds = gr::k_invalidRenderDataID;

            // Axis-Aligned Bounding Box (AABB) points
            glm::vec3 m_minXYZ;
            glm::vec3 m_maxXYZ;
        };
    };
}