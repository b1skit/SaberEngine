// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "RenderObjectIDs.h"
#include "ShadowMap.h"
#include "ShadowMapRenderData.h"


namespace fr
{
	class GameplayManager;


	class ShadowMapComponent
	{
	public:
		static ShadowMapComponent& AttachShadowMapComponent(
			GameplayManager&, entt::entity, char const* name, fr::Light::LightType);

		static gr::ShadowMap::RenderData CreateRenderData(fr::ShadowMapComponent const&);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
	public:
		ShadowMapComponent(PrivateCTORTag, gr::LightID, fr::Light::LightType);


		gr::LightID GetLightID() const;

	private:
		const gr::LightID m_lightID;

		fr::ShadowMap m_shadowMap;
	};


	// ---


	class UpdateShadowMapDataRenderCommand
	{
	public:
		UpdateShadowMapDataRenderCommand(ShadowMapComponent const&);

		static void Execute(void*);
		static void Destroy(void*);

	private:
		const gr::LightID m_lightID;

		const gr::ShadowMap::RenderData m_data;
	};


	// ---


	inline gr::LightID ShadowMapComponent::GetLightID() const
	{
		return m_lightID;
	}
}