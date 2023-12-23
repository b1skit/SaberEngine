// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "LightRenderData.h"
#include "MeshPrimitive.h"
#include "NamedObject.h"
#include "ParameterBlock.h"
#include "Transform.h"
#include "Updateable.h"


namespace re
{
	class Texture;
}

namespace fr
{
	class MeshPrimitive;
	class ShadowMap;


	class Light final : public virtual en::NamedObject, public virtual en::Updateable // ECS_CONVERSION: Remove inheritance
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

		static constexpr gr::Light::LightType GetRenderDataLightType(fr::Light::LightType);


	public:
		// DEPRECATED!!!!!!!!!!!!!
		static std::shared_ptr<fr::Light> CreateAmbientLight(std::string const& name);
		static std::shared_ptr<fr::Light> CreateDirectionalLight(
			std::string const& name, fr::Transform* ownerTransform, glm::vec4 colorIntensity, bool hasShadow);
		static std::shared_ptr<fr::Light> CreatePointLight(
			std::string const& name, fr::Transform* ownerTransform, glm::vec4 colorIntensity, bool hasShadow);


		Light(std::string const& name,
			fr::Transform* ownerTransform,
			LightType lightType,
			glm::vec4 colorIntensity,
			bool hasShadow);
		
		Light(re::Texture const* iblTex, LightType = LightType::AmbientIBL_Deferred); // Ambient light CTOR

		Light(fr::Light&&) = default;
		Light& operator=(fr::Light&&) = default;

		~Light() { Destroy(); }
		void Destroy();


		void Update(const double stepTimeMs) override; // DEPRECATED!!!!!!!!!!!!!

		glm::vec4 GetColorIntensity() const;
	 
		LightType const& GetType() const;
		
		
		void ShowImGuiWindow();


		// DEPRECATED!!!!!!!!!!!!!
		fr::Transform* GetTransform(); // Directional lights shine forward (Z+)

		// DEPRECATED!!!!!!!!!!!!!
		fr::ShadowMap* GetShadowMap() const;


	
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
					fr::Transform* m_ownerTransform; // DEPRECATED!!!!!!!!!!!!!
					
					glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
					std::unique_ptr<fr::ShadowMap> m_shadowMap; // DEPRECATED!!!!!!!!!!!!!

					std::shared_ptr<gr::MeshPrimitive> m_screenAlignedQuad; // DEPRECATED!!!!!!!!!!!!!
				} m_directional;
				struct
				{
					fr::Transform* m_ownerTransform; // DEPRECATED!!!!!!!!!!!!!
					
					glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
					float m_emitterRadius; // For non-singular attenuation function
					float m_intensityCuttoff; // Intensity value at which we stop drawing the deferred mesh
					
					std::shared_ptr<gr::MeshPrimitive> m_sphereMeshPrimitive; // DEPRECATED!!!!!!!!!!!!!
					std::unique_ptr<fr::ShadowMap> m_cubeShadowMap; // DEPRECATED!!!!!!!!!!!!!
				} m_point;
			};

			// Debug params:
			bool m_diffuseEnabled;
			bool m_specularEnabled;

			TypeProperties();
			~TypeProperties();
		};
		TypeProperties& AccessLightTypeProperties(LightType);
		TypeProperties const& AccessLightTypeProperties(LightType) const;


	private:		
		TypeProperties m_typeProperties;


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


	inline Light::LightType const& Light::GetType() const
	{
		return m_typeProperties.m_type;
	}


	inline fr::Transform* Light::GetTransform()
	{
		switch (m_typeProperties.m_type)
		{
		case LightType::AmbientIBL_Deferred:
		{
			SEAssertF("Ambient lights do not have a transform");
		}
		break;
		case LightType::Directional_Deferred:
		{
			// Note: Directional lights shine forward (Z+)
			return m_typeProperties.m_directional.m_ownerTransform;
		}
		break;
		case LightType::Point_Deferred:
		{
			return m_typeProperties.m_point.m_ownerTransform;
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}

		return nullptr;
	}
}