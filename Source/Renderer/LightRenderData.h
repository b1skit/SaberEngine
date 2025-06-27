// � 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"
#include "Texture.h"

#include "Core/Assert.h"
#include "Core/InvPtr.h"

#include "Core/Interfaces/INamedObject.h"



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
		static constexpr char const* LightTypeToCStr(Light::Type);


		struct RenderDataAmbientIBL
		{
			// Ordered from largest to smallest to reduce padding:
			char m_lightName[core::INamedObject::k_maxNameLength];
			core::InvPtr<re::Texture> m_iblTex;
			
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;

			float m_diffuseScale = 1.f;
			float m_specularScale = 1.f;

			bool m_isActive = false; // Note: Only *one* ambient light can be active at any time

		public:
			RenderDataAmbientIBL(char const* name, gr::RenderDataID, gr::TransformID);

		private:
			RenderDataAmbientIBL() = delete;
		};


		struct RenderDataDirectional
		{
			// Ordered from largest to smallest to reduce padding:
			char m_lightName[core::INamedObject::k_maxNameLength];
			
			glm::vec4 m_colorIntensity = glm::vec4(0.f); // .rgb = hue, .a = intensity
			
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;

			// 1-byte members grouped together
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
			// Ordered from largest to smallest to reduce padding:
			char m_lightName[core::INamedObject::k_maxNameLength];
			
			glm::vec4 m_colorIntensity = glm::vec4(0.f); // .rgb = hue, .a = intensity
			
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;
			
			float m_emitterRadius = 0.f; // For non-singular attenuation function
			float m_intensityCuttoff = 0.f; // Intensity value at which we stop drawing the deferred mesh
			float m_sphericalRadius = 0.f; // Derrived from m_colorIntensity, m_emitterRadius, m_intensityCuttoff

			// 1-byte members grouped together
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
			// Ordered from largest to smallest to reduce padding:
			char m_lightName[core::INamedObject::k_maxNameLength];
			
			glm::vec4 m_colorIntensity = glm::vec4(0.f); // .rgb = hue, .a = intensity
			
			gr::RenderDataID m_renderDataID;
			gr::TransformID m_transformID;
			
			float m_emitterRadius = 0.f; // For non-singular attenuation function
			float m_intensityCuttoff = 0.f; // Intensity value at which we stop drawing the deferred mesh
			float m_innerConeAngle = 0.f; // Radians: Angle from the center of the light where falloff begins
			float m_outerConeAngle = 0.f;
			float m_coneHeight = 0.f; // Derrived from m_colorIntensity, m_emitterRadius, m_intensityCuttoff

			// 1-byte members grouped together
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


	inline constexpr char const* Light::LightTypeToCStr(Light::Type lightType)
	{
		switch (lightType)
		{
			case AmbientIBL: return "AmbientIBL";
			case Directional: return "Directional";
			case Point: return "Point";
			case Spot: return "Spot";
			default: return "INVALID_LIGHT_TYPE";
		}
		SEStaticAssert(Light::Type_Count == 4, "Light type count changed. This must be updated");
	}
}