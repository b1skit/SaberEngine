// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "LightRenderData.h"


namespace fr
{
	class Camera;
	class ShadowMap;
	class Transform;


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
		static_assert(static_cast<uint8_t>(fr::Light::Type::Type_Count) ==
			static_cast<uint8_t>(gr::Light::Type::Type_Count));

		static constexpr gr::Light::Type ConvertRenderDataLightType(fr::Light::Type);


	public:
		Light(Type lightType, glm::vec4 const& colorIntensity);
		Light(re::Texture const* iblTex, Type = Type::AmbientIBL); // Ambient light only CTOR

		Light(fr::Light&&) = default;
		Light& operator=(fr::Light&&) = default;
		~Light() = default;

		bool Update();

		bool IsDirty() const;
		void MarkClean();

		glm::vec4 const& GetColorIntensity() const;
		void SetColorIntensity(glm::vec4 const&);
	 
		Type GetType() const;
		
		void ShowImGuiWindow(uint64_t uniqueID);

	
	public:
		struct TypeProperties
		{
			struct AmbientProperties
			{
				re::Texture const* m_IBLTex;

				float m_diffuseScale;
				float m_specularScale;
			};
			struct DirectionalProperties
			{
				// Note: Directional lights shine forward (Z+)									
				glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
			};
			struct PointProperties
			{
				glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
				float m_emitterRadius; // For non-singular attenuation function
				float m_intensityCuttoff; // Intensity value at which we stop drawing the deferred mesh

				float m_sphericalRadius; // Derrived from m_colorIntensity, m_emitterRadius, m_intensityCuttoff
			};

			Type m_type;
			union
			{
				AmbientProperties m_ambient;
				DirectionalProperties m_directional;
				PointProperties m_point;
			};

			// Debug params:
			bool m_diffuseEnabled;
			bool m_specularEnabled;

			TypeProperties();
			~TypeProperties() = default;
		};
		TypeProperties const& GetLightTypeProperties(Type) const;
		void SetLightTypeProperties(Type, void const*);


	private:		
		TypeProperties m_typeProperties;

		bool m_isDirty;


	private: // No copying allowed
		Light() = delete;
		Light(fr::Light const&) = delete;
		Light& operator=(fr::Light const&) = delete;
	};


	inline constexpr gr::Light::Type Light::ConvertRenderDataLightType(fr::Light::Type frLightType)
	{
		switch (frLightType)
		{
		case fr::Light::Type::AmbientIBL: return gr::Light::Type::AmbientIBL;
		case fr::Light::Type::Directional: return gr::Light::Type::Directional;
		case fr::Light::Type::Point: return gr::Light::Type::Point;
		default: throw std::logic_error("Invalid light type");
		}
		return gr::Light::Type::Type_Count;
	}


	inline Light::Type Light::GetType() const
	{
		return m_typeProperties.m_type;
	}


	inline bool Light::IsDirty() const
	{
		return m_isDirty;
	}


	inline void Light::MarkClean()
	{
		m_isDirty = false;
	}
}