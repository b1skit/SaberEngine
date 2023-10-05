// � 2022 Adam Badke. All rights reserved.
#pragma once

#include "Mesh.h"
#include "NamedObject.h"
#include "ParameterBlock.h"
#include "ShadowMap.h"
#include "Transform.h"
#include "Updateable.h"


namespace gr
{
	class Camera;
	class MeshPrimitive;
}

namespace re
{
	class Shader;
}

namespace gr
{
	class Light final : public virtual en::NamedObject, public virtual en::Updateable
	{
	public:
		enum LightType : uint8_t
		{
			// Note: Currently, these are all deferred types
			AmbientIBL,
			Directional,
			Point,

			//Spot,
			//Area,
			//Tube,

			Light_Count
		};

	public:
		static std::shared_ptr<Light> CreateAmbientLight(std::string const& name);

		static std::shared_ptr<Light> CreateDirectionalLight(
			std::string const& name, gr::Transform* ownerTransform, glm::vec3 colorIntensity, bool hasShadow);

		static std::shared_ptr<Light> CreatePointLight(
			std::string const& name, gr::Transform* ownerTransform, glm::vec3 colorIntensity, bool hasShadow);

		~Light() { Destroy(); }

		Light(Light&&) = default;
		Light& operator=(Light&&) = default;

		void Destroy();

		void Update(const double stepTimeMs) override;

		glm::vec3 GetColor() const;
	 
		LightType const& Type() const;
														 
		gr::Transform* GetTransform(); // Directional lights shine forward (Z+)

		gr::ShadowMap* GetShadowMap() const;

		void ShowImGuiWindow();

	
	public:
		struct LightTypeProperties
		{
			union
			{
				struct
				{
					std::shared_ptr<re::Texture> m_BRDF_integrationMap;
					std::shared_ptr<re::Texture> m_IEMTex;
					std::shared_ptr<re::Texture> m_PMREMTex;
				} m_ambient;
				struct
				{
					gr::Transform* m_ownerTransform;
					glm::vec3 m_colorIntensity;
					std::unique_ptr<gr::ShadowMap> m_shadowMap;
				} m_directional;
				struct
				{
					gr::Transform* m_ownerTransform;
					glm::vec3 m_colorIntensity;
					std::unique_ptr<gr::ShadowMap> m_cubeShadowMap;
				} m_point;
			};

			// Debug params:
			float m_intensityScale;
			bool m_diffuseEnabled;
			bool m_specularEnabled;

			LightTypeProperties()
			{
				memset(this, 0, sizeof(LightTypeProperties));
				m_intensityScale = 1.f;
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


	private: // Use one of the Create() factories
		Light(std::string const& name,
			gr::Transform* ownerTransform,
			LightType lightType,
			glm::vec3 colorIntensity,
			bool hasShadow);

	private:
		Light() = delete;
		Light(Light const&) = delete;
		Light& operator=(Light const&) = delete;
	};


	inline Light::LightType const& Light::Type() const
	{
		return m_type;
	}


	inline gr::Transform* Light::GetTransform()
	{
		switch (m_type)
		{
		case LightType::AmbientIBL:
		{
			SEAssertF("Ambient lights do not have a transform");
		}
		break;
		case LightType::Directional:
		{
			return m_typeProperties.m_directional.m_ownerTransform;
		}
		break;
		case LightType::Point:
		{
			return m_typeProperties.m_point.m_ownerTransform;
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}

		return nullptr;
	}	// Directional lights shine forward (Z+)
}