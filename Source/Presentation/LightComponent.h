// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Light.h"
#include "NameComponent.h"

#include "Renderer/LightRenderData.h"
#include "Renderer/RenderObjectIDs.h"


namespace re
{
	class Texture;
}

namespace fr
{
	class Camera;
	class EntityManager;
	class RenderDataComponent;
	class Transform;


	class LightComponent final
	{
	public:
		struct AmbientIBLDeferredMarker final {};
		struct IsActiveAmbientDeferredMarker final {};

		struct PointDeferredMarker final {};
		struct SpotDeferredMarker final {};
		struct DirectionalDeferredMarker final {};


	public:
		static entt::entity CreateDeferredAmbientLightConcept(EntityManager&, std::string_view name, core::InvPtr<re::Texture> const& iblTex);

		static LightComponent& AttachDeferredPointLightConcept(
			fr::EntityManager&, entt::entity, std::string_view name, glm::vec4 const& colorIntensity, bool hasShadow);

		static LightComponent& AttachDeferredSpotLightConcept(
			fr::EntityManager&, entt::entity, std::string_view name, glm::vec4 const& colorIntensity, bool hasShadow);

		static LightComponent& AttachDeferredDirectionalLightConcept(
			fr::EntityManager&, entt::entity, std::string_view name, glm::vec4 const& colorIntensity, bool hasShadow);

	public:
		static gr::Light::RenderDataAmbientIBL CreateRenderDataAmbientIBL_Deferred(
			fr::NameComponent const&, fr::LightComponent const&);
		
		static gr::Light::RenderDataDirectional CreateRenderDataDirectional_Deferred(
			fr::NameComponent const&, fr::LightComponent const&);
		
		static gr::Light::RenderDataPoint CreateRenderDataPoint_Deferred(
			fr::NameComponent const&, fr::LightComponent const&);

		static gr::Light::RenderDataSpot CreateRenderDataSpot_Deferred(
			fr::NameComponent const&, fr::LightComponent const&);

		static void Update(entt::entity, fr::LightComponent&, fr::Transform* lightTransform, fr::Camera* shadowCam);

		static void ShowImGuiWindow(fr::EntityManager&, entt::entity lightEntity);
		static void ShowImGuiSpawnWindow();


	public:
		gr::RenderDataID GetRenderDataID() const;
		gr::TransformID GetTransformID() const;

		fr::Light& GetLight();
		fr::Light const& GetLight() const;


	private:
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;

	private:
		fr::Light m_light;
		const bool m_hasShadow;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		LightComponent(
			PrivateCTORTag, 
			fr::RenderDataComponent const&, 
			fr::Light::Type, 
			glm::vec4 colorIntensity,
			bool hasShadow);
		LightComponent(
			PrivateCTORTag, 
			fr::RenderDataComponent const&,
			core::InvPtr<re::Texture> const& iblTex,
			const fr::Light::Type = fr::Light::Type::AmbientIBL); // Ambient light only
	};


	// ---


	class UpdateLightDataRenderCommand final
	{
	public:
		UpdateLightDataRenderCommand(fr::NameComponent const&, LightComponent const&);
		~UpdateLightDataRenderCommand();

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;

		gr::Light::Type m_type;
		union
		{
			gr::Light::RenderDataAmbientIBL m_ambientData;
			gr::Light::RenderDataDirectional m_directionalData;
			gr::Light::RenderDataPoint m_pointData;
			gr::Light::RenderDataSpot m_spotData;
		};
	};


	class DestroyLightDataRenderCommand final
	{
	public:
		DestroyLightDataRenderCommand(LightComponent const&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderDataID m_renderDataID;
		const gr::Light::Type m_type;
	};


	// ---


	inline gr::RenderDataID LightComponent::GetRenderDataID() const
	{
		return m_renderDataID;
	}


	inline gr::TransformID LightComponent::GetTransformID() const
	{
		return m_transformID;
	}


	inline fr::Light& LightComponent::GetLight()
	{
		return m_light;
	}


	inline fr::Light const& LightComponent::GetLight() const
	{
		return m_light;
	}
}