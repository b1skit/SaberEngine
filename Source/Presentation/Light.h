// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Renderer/LightRenderData.h"


namespace fr
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
		static_assert(static_cast<uint8_t>(fr::Light::Type::Type_Count) ==
			static_cast<uint8_t>(gr::Light::Type::Type_Count));

		static constexpr std::array<char const*, fr::Light::Type::Type_Count> k_lightTypeNames = {
			"Ambient Light",
			"Directional Light",
			"Point Light",
			"Spot Light",
		};
		static_assert(k_lightTypeNames.size() == fr::Light::Type::Type_Count);


		static constexpr gr::Light::Type ConvertToGrLightType(fr::Light::Type);


	public:
		Light(Type lightType, glm::vec4 const& colorIntensity);
		Light(core::InvPtr<re::Texture> const& iblTex, Type = Type::AmbientIBL); // Ambient light only CTOR

		Light(fr::Light&&) noexcept = default;
		Light& operator=(fr::Light&&) noexcept = default;
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
				core::InvPtr<re::Texture> m_IBLTex;

				bool m_isActive; // Note: Only *one* ambient light can be active at any time

				float m_diffuseScale;
				float m_specularScale;
			};

			struct DirectionalProperties
			{
				glm::vec4 m_colorIntensity; // .rgb = hue, .a = luminous power (phi)
			};
			struct PointProperties
			{
				glm::vec4 m_colorIntensity; // .rgb = hue, .a = luminous power (phi)
				float m_emitterRadius; // For non-singular attenuation function
				float m_intensityCuttoff; // Intensity value at which the light's contribution is considered to be 0

				float m_sphericalRadius; // Derrived from m_colorIntensity, m_emitterRadius, m_intensityCuttoff
			};
			struct SpotProperties
			{
				glm::vec4 m_colorIntensity; // .rgb = hue, .a = luminous power (phi)
				float m_emitterRadius; // For non-singular attenuation function
				float m_intensityCuttoff; // Intensity value at which the light's contribution is considered to be 0
				
				float m_innerConeAngle; // Radians: Angle from the center of the light where falloff begins
				float m_outerConeAngle;
				float m_coneHeight; // Derrived from m_colorIntensity, m_emitterRadius, m_intensityCuttoff
			};

			Type m_type;
			union
			{
				AmbientProperties m_ambient;
				DirectionalProperties m_directional;
				PointProperties m_point;
				SpotProperties m_spot;
			};

			// Debug params:
			bool m_diffuseEnabled;
			bool m_specularEnabled;

			TypeProperties();
			TypeProperties(TypeProperties const&) noexcept;
			TypeProperties(TypeProperties&&) noexcept;
			TypeProperties& operator=(TypeProperties const&) noexcept;
			TypeProperties& operator=(TypeProperties&&) noexcept;
			~TypeProperties();
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


	inline constexpr gr::Light::Type Light::ConvertToGrLightType(fr::Light::Type frLightType)
	{
		switch (frLightType)
		{
		case fr::Light::Type::AmbientIBL: return gr::Light::Type::AmbientIBL;
		case fr::Light::Type::Directional: return gr::Light::Type::Directional;
		case fr::Light::Type::Point: return gr::Light::Type::Point;
		case fr::Light::Type::Spot: return gr::Light::Type::Spot;
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