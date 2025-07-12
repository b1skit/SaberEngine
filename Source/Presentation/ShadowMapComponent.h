// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "ShadowMap.h"
#include "TransformComponent.h"

#include "Renderer/RenderObjectIDs.h"


namespace pr
{
	class BoundsComponent;
	class CameraComponent;
	class EntityManager;
	class LightComponent;


	class ShadowMapComponent
	{
	public:
		struct HasShadowMarker {};


	public:
		// Note: May trigger a .Recompute() of the entity's owning Transform
		static ShadowMapComponent& AttachShadowMapComponent(
			EntityManager&, entt::entity, char const* name, pr::Light::Type);

	private:
		static gr::Camera::Config SnapTransformAndGenerateShadowCameraConfig(
			ShadowMap const&,
			pr::Transform&,
			pr::Light const&,
			pr::BoundsComponent const* sceneWorldBounds,
			pr::CameraComponent const* activeSceneCam);

	public:
		static gr::ShadowMap::RenderData CreateRenderData(entt::entity, pr::ShadowMapComponent const&);


		static void Update(
			entt::entity,
			pr::ShadowMapComponent&,
			pr::TransformComponent& lightTransformCmpt,
			pr::LightComponent const&,
			pr::CameraComponent&,
			pr::BoundsComponent const* sceneWorldBounds, // Optional
			pr::CameraComponent const* activeSceneCam, // Optional
			bool force);

		static void ShowImGuiWindow(pr::EntityManager&, entt::entity shadowMapEntity);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		ShadowMapComponent(
			PrivateCTORTag,
			pr::Light::Type, 
			gr::RenderDataID, 
			gr::TransformID);

		gr::RenderDataID GetRenderDataID() const;
		gr::TransformID GetTransformID() const;

		pr::ShadowMap& GetShadowMap();
		pr::ShadowMap const& GetShadowMap() const;


	private:
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;

		pr::ShadowMap m_shadowMap;
	};


	inline gr::RenderDataID ShadowMapComponent::GetRenderDataID() const
	{
		return m_renderDataID;
	}


	inline gr::TransformID ShadowMapComponent::GetTransformID() const
	{
		return m_transformID;
	}


	inline pr::ShadowMap& ShadowMapComponent::GetShadowMap()
	{
		return m_shadowMap;
	}


	inline pr::ShadowMap const& ShadowMapComponent::GetShadowMap() const
	{
		return m_shadowMap;
	}
}