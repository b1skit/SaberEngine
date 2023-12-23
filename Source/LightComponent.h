// � 2023 Adam Badke. All rights reserved.
#pragma once
#include "Light.h"
#include "LightRenderData.h"
#include "RenderObjectIDs.h"


namespace gr
{
	class RenderDataComponent;
}

namespace re
{
	class Texture;
}

namespace fr
{
	class LightComponent
	{
	public:
		static LightComponent& CreateDeferredAmbientLightConcept(re::Texture const* iblTex);

		static LightComponent& AttachDeferredPointLightConcept(
			entt::entity, char const* name, glm::vec4 colorIntensity, bool hasShadow);

		static LightComponent& AttachDeferredDirectionalLightConcept(
			entt::entity, char const* name, glm::vec4 colorIntensity, bool hasShadow);

		static gr::Light::RenderData CreateRenderData(fr::LightComponent const&);


	public:
		gr::LightID GetLightID() const;
		gr::LightID GetRenderDataID() const;
		gr::LightID GetTransformID() const;

		fr::Light& GetLight();


	private:
		const gr::LightID m_lightID;
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;

	private:
		fr::Light m_light;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		LightComponent(PrivateCTORTag, gr::RenderDataComponent const&, fr::Light::LightType, glm::vec4 colorIntensity);
		LightComponent(
			PrivateCTORTag, 
			gr::RenderDataComponent const&,
			re::Texture const* iblTex,
			const fr::Light::LightType = fr::Light::LightType::AmbientIBL_Deferred); // Ambient light only


	private: // Static LightID functionality:
		static std::atomic<uint32_t> s_lightIDs;
	};


	// ---


	class UpdateLightDataRenderCommand
	{
	public:
		UpdateLightDataRenderCommand(LightComponent const&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::LightID m_lightID;
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;

		const gr::Light::RenderData m_data;
	};


	// ---


	inline gr::LightID LightComponent::GetLightID() const
	{
		return m_lightID;
	}


	inline gr::LightID LightComponent::GetRenderDataID() const
	{
		return m_renderDataID;
	}


	inline gr::LightID LightComponent::GetTransformID() const
	{
		return m_transformID;
	}


	inline fr::Light& LightComponent::GetLight()
	{
		return m_light;
	}
}