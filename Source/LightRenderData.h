// © 2023 Adam Badke. All rights reserved.
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
		enum Type : uint8_t
		{
			AmbientIBL,
			Directional,
			Point,

			Type_Count
		};


		struct RenderDataAmbientIBL
		{
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;

			char m_lightName[en::NamedObject::k_maxNameLength];

			// Ambient type data:
			re::Texture const* m_iblTex = nullptr;

			bool m_isActive; // Note: Only *one* ambient light can be active at any time

			float m_diffuseScale = 1.f;
			float m_specularScale = 1.f;

		public:
			RenderDataAmbientIBL(char const* name, gr::RenderDataID, gr::TransformID);

		private:
			RenderDataAmbientIBL() = delete;
		};


		struct RenderDataDirectional
		{
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;

			char m_lightName[en::NamedObject::k_maxNameLength];

			// Directional type data:
			glm::vec4 m_colorIntensity = glm::vec4(0.f); // .rgb = hue, .a = intensity

			bool m_hasShadow = false;

			// Debug params:
			bool m_diffuseEnabled = false;
			bool m_specularEnabled = false;

		public:
			RenderDataDirectional(char const* name, gr::RenderDataID, gr::TransformID);

		private:
			RenderDataDirectional() = delete;
		};


		struct RenderDataPoint
		{
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;

			char m_lightName[en::NamedObject::k_maxNameLength];

			// Point type data:
			glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
			float m_emitterRadius; // For non-singular attenuation function
			float m_intensityCuttoff; // Intensity value at which we stop drawing the deferred mesh

			float m_sphericalRadius; // Derrived from m_colorIntensity, m_emitterRadius, m_intensityCuttoff

			bool m_hasShadow = false;

			// Debug params:
			bool m_diffuseEnabled = false;
			bool m_specularEnabled = false;

		public:
			RenderDataPoint(char const* name, gr::RenderDataID, gr::TransformID);

		private:
			RenderDataPoint() = delete;
		};
	};
}