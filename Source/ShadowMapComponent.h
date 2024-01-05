// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"
#include "ShadowMap.h"
#include "ShadowMapRenderData.h"


namespace fr
{
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
			EntityManager&, entt::entity, char const* name, fr::Light::Type);

	public:
		static gr::Camera::Config GenerateShadowCameraConfig(
			ShadowMap const&, fr::Transform const&, fr::Light const&, fr::BoundsComponent const*);

		static gr::ShadowMap::RenderData CreateRenderData(
			fr::NameComponent const& nameCmpt, fr::ShadowMapComponent const&);


		static bool Update( // Returns true if modified (or forced to modify)
			fr::ShadowMapComponent&,
			fr::TransformComponent const& lightTransformCmpt,
			fr::LightComponent const&,
			fr::CameraComponent&,
			fr::BoundsComponent const* sceneWorldBounds, // Optional
			bool force);

		static void ShowImGuiWindow(fr::EntityManager&, entt::entity shadowMapEntity);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		ShadowMapComponent(
			PrivateCTORTag,
			fr::Light::Type, 
			gr::RenderDataID, 
			gr::TransformID, 
			glm::uvec2 widthHeight);

		gr::RenderDataID GetRenderDataID() const;
		gr::TransformID GetTransformID() const;

		fr::ShadowMap& GetShadowMap();
		fr::ShadowMap const& GetShadowMap() const;


	private:
		const gr::RenderDataID m_renderDataID;
		const gr::TransformID m_transformID;

		fr::ShadowMap m_shadowMap;
	};


	// ---


	class UpdateShadowMapDataRenderCommand
	{
	public:
		UpdateShadowMapDataRenderCommand(fr::NameComponent const& nameCmpt, ShadowMapComponent const&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderDataID m_renderDataID;

		const gr::Light::Type m_type;

		const gr::ShadowMap::RenderData m_data;
	};


	class DestroyShadowMapDataRenderCommand
	{
	public:
		DestroyShadowMapDataRenderCommand(ShadowMapComponent const&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::RenderDataID m_renderDataID;
		
		const gr::Light::Type m_type;
	};


	// ---


	inline gr::RenderDataID ShadowMapComponent::GetRenderDataID() const
	{
		return m_renderDataID;
	}


	inline gr::TransformID ShadowMapComponent::GetTransformID() const
	{
		return m_transformID;
	}


	inline fr::ShadowMap& ShadowMapComponent::GetShadowMap()
	{
		return m_shadowMap;
	}


	inline fr::ShadowMap const& ShadowMapComponent::GetShadowMap() const
	{
		return m_shadowMap;
	}
}