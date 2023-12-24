// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "LightRenderData.h"


namespace fr
{
	class Camera;
	class Transform;


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
		static_assert(static_cast<uint8_t>(fr::Light::LightType::LightType_Count) ==
			static_cast<uint8_t>(gr::Light::LightType::LightType_Count));

	public:
		static constexpr gr::Light::LightType GetRenderDataLightType(fr::Light::LightType);

		static void ConfigurePointLightMeshScale(fr::Light&, fr::Transform&, fr::Camera* shadowCam);


	public:
		Light(LightType lightType, glm::vec4 const& colorIntensity);
		Light(re::Texture const* iblTex, LightType = LightType::AmbientIBL_Deferred); // Ambient light only CTOR

		Light(fr::Light&&) = default;
		Light& operator=(fr::Light&&) = default;

		~Light() { Destroy(); }
		void Destroy();

		bool IsDirty() const;
		void MarkClean();

		glm::vec4 const& GetColorIntensity() const;
		void SetColorIntensity(glm::vec4 const&);
	 
		LightType GetType() const;
		
		
		void ShowImGuiWindow();

	
	public:
		struct TypeProperties
		{
			LightType m_type;
			union
			{
				struct
				{
					re::Texture const* m_IBLTex;

				} m_ambient;
				struct
				{
					// Note: Directional lights shine forward (Z+)									
					glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
				} m_directional;
				struct
				{					
					glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
					float m_emitterRadius; // For non-singular attenuation function
					float m_intensityCuttoff; // Intensity value at which we stop drawing the deferred mesh
				} m_point;
			};

			// Debug params:
			bool m_diffuseEnabled;
			bool m_specularEnabled;

			TypeProperties();
			~TypeProperties() = default;
		};
		TypeProperties& GetLightTypePropertiesForModification(LightType);
		TypeProperties const& GetLightTypeProperties(LightType) const;


	private:		
		TypeProperties m_typeProperties;

		bool m_isDirty;


	private: // No copying allowed
		Light() = delete;
		Light(fr::Light const&) = delete;
		Light& operator=(fr::Light const&) = delete;
	};


	inline constexpr gr::Light::LightType Light::GetRenderDataLightType(fr::Light::LightType frLightType)
	{
		switch (frLightType)
		{
		case fr::Light::LightType::AmbientIBL_Deferred: return gr::Light::LightType::AmbientIBL_Deferred;
		case fr::Light::LightType::Directional_Deferred: return gr::Light::LightType::Directional_Deferred;
		case fr::Light::LightType::Point_Deferred: return gr::Light::LightType::Point_Deferred;
		default: throw std::logic_error("Invalid light type");
		}
		return gr::Light::LightType::LightType_Count;
	}


	inline Light::LightType Light::GetType() const
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