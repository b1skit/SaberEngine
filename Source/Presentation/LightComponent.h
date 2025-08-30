// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Light.h"
#include "NameComponent.h"

#include "Renderer/LightRenderData.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/RenderObjectIDs.h"


namespace re
{
	class Texture;
}

namespace pr
{
	class Camera;
	class EntityManager;
	class RenderDataComponent;
	class Transform;


	class LightComponent final
	{
	public:
		struct IBLDeferredMarker final {};
		struct IsActiveIBLMarker final {};

		struct PointDeferredMarker final {};
		struct SpotDeferredMarker final {};
		struct DirectionalDeferredMarker final {};


	public:
		static entt::entity CreateImageBasedLightConcept(EntityManager&, std::string_view name, core::InvPtr<re::Texture> const&);

		static LightComponent& AttachDeferredPointLightConcept(
			pr::EntityManager&, entt::entity, std::string_view name, glm::vec4 const& colorIntensity, bool hasShadow);

		static LightComponent& AttachDeferredSpotLightConcept(
			pr::EntityManager&, entt::entity, std::string_view name, glm::vec4 const& colorIntensity, bool hasShadow);

		static LightComponent& AttachDeferredDirectionalLightConcept(
			pr::EntityManager&, entt::entity, std::string_view name, glm::vec4 const& colorIntensity, bool hasShadow);

	public:
		static gr::Light::RenderDataIBL CreateRenderDataAmbientIBL_Deferred(
			pr::NameComponent const&, pr::LightComponent const&);
		
		static gr::Light::RenderDataDirectional CreateRenderDataDirectional_Deferred(
			pr::NameComponent const&, pr::LightComponent const&);
		
		static gr::Light::RenderDataPoint CreateRenderDataPoint_Deferred(
			pr::NameComponent const&, pr::LightComponent const&);

		static gr::Light::RenderDataSpot CreateRenderDataSpot_Deferred(
			pr::NameComponent const&, pr::LightComponent const&);

		static void Update(pr::EntityManager&, entt::entity, pr::LightComponent&, pr::Transform* lightTransform, pr::Camera* shadowCam);

		static void ShowImGuiWindow(pr::EntityManager&, entt::entity lightEntity);
		static void ShowImGuiSpawnWindow(pr::EntityManager& em);


	public:
		gr::RenderDataID GetRenderDataID() const;
		gr::TransformID GetTransformID() const;

		pr::Light& GetLight();
		pr::Light const& GetLight() const;


	private:
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;

	private:
		pr::Light m_light;
		const bool m_hasShadow;


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		LightComponent(
			PrivateCTORTag, 
			pr::RenderDataComponent const&, 
			pr::Light::Type, 
			glm::vec4 colorIntensity,
			bool hasShadow);
		LightComponent(
			PrivateCTORTag, 
			pr::RenderDataComponent const&,
			core::InvPtr<re::Texture> const& iblTex,
			const pr::Light::Type = pr::Light::Type::IBL); // Ambient light only
	};


	// ---


	class UpdateLightDataRenderCommand final : public virtual gr::RenderCommand
	{
	public:
		UpdateLightDataRenderCommand(pr::NameComponent const&, LightComponent const&);
		~UpdateLightDataRenderCommand();

		static void Execute(void*);

	private:
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;

		gr::Light::Type m_type;
		union
		{
			gr::Light::RenderDataIBL m_ambientData;
			gr::Light::RenderDataDirectional m_directionalData;
			gr::Light::RenderDataPoint m_pointData;
			gr::Light::RenderDataSpot m_spotData;
		};
	};


	class DestroyLightDataRenderCommand final : public virtual gr::RenderCommand
	{
	public:
		DestroyLightDataRenderCommand(LightComponent const&);

		static void Execute(void*);

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


	inline pr::Light& LightComponent::GetLight()
	{
		return m_light;
	}


	inline pr::Light const& LightComponent::GetLight() const
	{
		return m_light;
	}
}