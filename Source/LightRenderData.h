// � 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"
#include "NameComponent.h"


namespace re
{
	class Texture;
}

namespace gr
{
	class Light
	{
	public:
		enum LightType : uint8_t
		{
			AmbientIBL_Deferred,
			Directional_Deferred,
			Point_Deferred,

			LightType_Count
		};


		struct RenderData
		{
			// Identifiers:
			gr::Light::LightType m_lightType;

			gr::LightID m_lightID;
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;

			char m_lightName[en::NamedObject::k_maxNameLength];

			// Type-specific light data:
			union
			{
				struct
				{
					re::Texture const* m_iblTex;
					
				} m_ambient;
				struct
				{
					glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity

					bool m_hasShadow;
				} m_directional;
				struct
				{
					glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
					float m_emitterRadius; // For non-singular attenuation function
					float m_intensityCuttoff; // Intensity value at which we stop drawing the deferred mesh
					
					float m_sphericalRadius; // Derrived from m_colorIntensity, m_emitterRadius, m_intensityCuttoff

					bool m_hasShadow;
				} m_point;
			} m_typeProperties;

			// Debug params:
			bool m_diffuseEnabled;
			bool m_specularEnabled;


		public:
			RenderData(
				char const* name, gr::Light::LightType, gr::LightID, gr::RenderDataID, gr::TransformID);
			~RenderData();
			RenderData& operator=(RenderData const&);


		private:
			RenderData() = delete;
		};
	};
}