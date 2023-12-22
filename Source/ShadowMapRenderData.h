// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "LightRenderData.h"
#include "RenderObjectIDs.h"


namespace gr
{
	class ShadowMap
	{
	public:
		enum class ShadowType : uint8_t
		{
			Orthographic, // Single 2D texture
			CubeMap,

			ShadowType_Count
		};


	public:
		struct RenderData
		{
			gr::LightID m_owningLightID;

			gr::Light::LightType m_lightType;
			ShadowType m_shadowType;

			glm::vec2 m_minMaxShadowBias;

			bool m_enabled;
		};

	};
}