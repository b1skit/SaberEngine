// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"

#include "Core/Interfaces/INamedObject.h"


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
			Spot,

			Type_Count
		};


		struct RenderDataAmbientIBL
		{
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;

			char m_lightName[core::INamedObject::k_maxNameLength];

			// Ambient type data:
			re::Texture const* m_iblTex = nullptr;

			bool m_isActive = false; // Note: Only *one* ambient light can be active at any time

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

			char m_lightName[core::INamedObject::k_maxNameLength];

			// Directional type data:
			glm::vec4 m_colorIntensity = glm::vec4(0.f); // .rgb = hue, .a = intensity

			bool m_hasShadow = false;

			bool m_canContribute = true; // True if color != black, intensity > 0, and debug flags are enabled

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

			char m_lightName[core::INamedObject::k_maxNameLength];

			// Point type data:
			glm::vec4 m_colorIntensity = glm::vec4(0.f); // .rgb = hue, .a = intensity
			float m_emitterRadius = 0.f; // For non-singular attenuation function
			float m_intensityCuttoff = 0.f; // Intensity value at which we stop drawing the deferred mesh

			float m_sphericalRadius = 0.f; // Derrived from m_colorIntensity, m_emitterRadius, m_intensityCuttoff

			bool m_hasShadow = false;

			bool m_canContribute = true; // True if color != black, intensity > 0, and debug flags are enabled

			// Debug params:
			bool m_diffuseEnabled = false;
			bool m_specularEnabled = false;

		public:
			RenderDataPoint(char const* name, gr::RenderDataID, gr::TransformID);

		private:
			RenderDataPoint() = delete;
		};


		struct RenderDataSpot
		{
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;

			char m_lightName[core::INamedObject::k_maxNameLength];

			// Spot type data:
			glm::vec4 m_colorIntensity = glm::vec4(0.f); // .rgb = hue, .a = intensity
			float m_emitterRadius = 0.f; // For non-singular attenuation function
			float m_intensityCuttoff = 0.f; // Intensity value at which we stop drawing the deferred mesh

			float m_innerConeAngle = 0.f; // Radians: Angle from the center of the light where falloff begins
			float m_outerConeAngle = 0.f;
			float m_coneHeight = 0.f; // Derrived from m_colorIntensity, m_emitterRadius, m_intensityCuttoff

			bool m_hasShadow = false;

			bool m_canContribute = true; // True if color != black, intensity > 0, and debug flags are enabled

			// Debug params:
			bool m_diffuseEnabled = false;
			bool m_specularEnabled = false;

		public:
			RenderDataSpot(char const* name, gr::RenderDataID, gr::TransformID);

		private:
			RenderDataSpot() = delete;
		};
	};
}