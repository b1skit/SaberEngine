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


	class Light final : public virtual en::NamedObject, public virtual en::Updateable
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
		struct LightTypeProperties
		{
			union
			{
				// ECS_CONVERSION TODO: All of these should be raw const* !!!!!!!!!!!!!!!!!!!!!!!!

				struct
				{
					std::shared_ptr<re::Texture> m_BRDF_integrationMap;
					std::shared_ptr<re::Texture> m_IEMTex;
					std::shared_ptr<re::Texture> m_PMREMTex;
					std::shared_ptr<gr::MeshPrimitive> m_screenAlignedQuad;
				} m_ambient;
				struct
				{
					fr::Transform* m_ownerTransform; // DEPRECATED!!!!!!!!!!!!!
					glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
					std::unique_ptr<fr::ShadowMap> m_shadowMap;
					std::shared_ptr<gr::MeshPrimitive> m_screenAlignedQuad;
				} m_directional;
				struct
				{
					fr::Transform* m_ownerTransform; // DEPRECATED!!!!!!!!!!!!!
					glm::vec4 m_colorIntensity; // .rgb = hue, .a = intensity
					float m_emitterRadius; // For non-singular attenuation function
					float m_intensityCuttoff; // Intensity value at which we stop drawing the deferred mesh
					std::shared_ptr<gr::MeshPrimitive> m_sphereMeshPrimitive;
					std::unique_ptr<fr::ShadowMap> m_cubeShadowMap;
				} m_point;
			};

			// Debug params:
			bool m_diffuseEnabled;
			bool m_specularEnabled;

			LightTypeProperties()
			{
				memset(this, 0, sizeof(LightTypeProperties));

				m_diffuseEnabled = true;
				m_specularEnabled = true;
			}

			~LightTypeProperties() {};
		};
		LightTypeProperties& AccessLightTypeProperties(LightType);
		LightTypeProperties const& AccessLightTypeProperties(LightType) const;


	private:
		LightType m_type;
		LightTypeProperties m_typeProperties;


	private:
		Light() = delete;
		Light(fr::Light const&) = delete;
		Light& operator=(fr::Light const&) = delete;
	};


	inline Light::LightType const& Light::GetType() const
	{
		return m_type;
	}


	inline fr::Transform* Light::GetTransform()
	{
		switch (m_type)
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