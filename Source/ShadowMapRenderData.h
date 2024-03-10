// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "LightRenderData.h"
#include "NamedObject.h"
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

		enum class ShadowQuality : uint8_t
		{
			PCF = 0,
			PCSS_LOW = 1,
			PCSS_HIGH = 2,

			ShadowQuality_Count
		};


	public:
		struct RenderData
		{
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;

			gr::Light::Type m_lightType;
			ShadowType m_shadowType;
			ShadowQuality m_shadowQuality;

			glm::vec4 m_textureDims;

			glm::vec2 m_minMaxShadowBias;
			float m_softness; // PCSS light size [0,1]

			bool m_shadowEnabled;

			char m_owningLightName[en::NamedObject::k_maxNameLength];
		};
	};
}