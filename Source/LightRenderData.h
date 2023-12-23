// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"


namespace re
{
	class Texture;
}

namespace gr
{
	class MeshPrimitive;


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
			gr::Light::LightType m_lightType;

			gr::LightID m_lightID;
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;

			union
			{
				struct
				{
					re::Texture const* m_iblTex;
					
				} m_ambient;
				struct
				{
					glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
				} m_directional;
				struct
				{
					glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
					float m_emitterRadius; // For non-singular attenuation function
					float m_intensityCuttoff; // Intensity value at which we stop drawing the deferred mesh
				} m_point;
			} m_typeProperties;

			// Debug params:
			bool m_diffuseEnabled;
			bool m_specularEnabled;

			RenderData(gr::Light::LightType, gr::LightID, gr::RenderDataID, gr::TransformID);
			~RenderData();
			RenderData& operator=(RenderData const&);


		private:
			RenderData() = delete;
		};
	};
}