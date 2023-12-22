// © 2023 Adam Badke. All rights reserved.
#include "Config.h"
#include "GameplayManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "RelationshipComponent.h"
#include "RenderManager.h"
#include "ShadowMapComponent.h"
#include "TransformComponent.h"


namespace fr
{
	ShadowMapComponent& ShadowMapComponent::AttachShadowMapComponent(
		GameplayManager& gpm, entt::entity owningEntity, char const* name, fr::Light::LightType lightType)
	{
		SEAssert("A shadow map's owning entity requires a Relationship component",
			gpm.HasComponent<fr::Relationship>(owningEntity));
		SEAssert("A shadow map requires a TransformComponent",
			fr::Relationship::IsInHierarchyAbove<fr::TransformComponent>(owningEntity));
		SEAssert("A ShadowMapComponent must be attached to a LightComponent",
			gpm.HasComponent<fr::LightComponent>(owningEntity));

		entt::entity shadowMapEntity = gpm.CreateEntity(name);

		// Relationship:
		fr::Relationship& shadowMapRelationship = fr::Relationship::AttachRelationshipComponent(gpm, shadowMapEntity);
		shadowMapRelationship.SetParent(gpm, owningEntity);

		// ShadowMap component:
		fr::LightComponent const& owningLightComponent = gpm.GetComponent<fr::LightComponent>(owningEntity);

		ShadowMapComponent& shadowMapComponent = *gpm.EmplaceComponent<fr::ShadowMapComponent>(
			shadowMapEntity, PrivateCTORTag{}, owningLightComponent.GetLightID(), lightType);

		// Mark our new ShadowMapComponent as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::ShadowMapComponent>>(shadowMapEntity);

		return shadowMapComponent;
	}


	gr::ShadowMap::RenderData ShadowMapComponent::CreateRenderData(fr::ShadowMapComponent const& shadowMapCmpt)
	{
		SEAssertF("TODO");
		// TODO....

		return gr::ShadowMap::RenderData{};
	}


	ShadowMapComponent::ShadowMapComponent(PrivateCTORTag, gr::LightID lightID, fr::Light::LightType lightType)
		: m_lightID(lightID)
		, m_shadowMap(
			"NAME IS DEPRECATED!!!!!!!!!!!!!!",
			en::Config::Get()->GetValue<int>(en::ConfigKeys::k_defaultShadowMapResolution),
			en::Config::Get()->GetValue<int>(en::ConfigKeys::k_defaultShadowMapResolution),
			nullptr, // shadowCamParent IS DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			lightType,
			nullptr) // owningTransform IS DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	{
	}


	// ---


	UpdateShadowMapDataRenderCommand::UpdateShadowMapDataRenderCommand(ShadowMapComponent const& shadowMapCmpt)
		: m_lightID(shadowMapCmpt.GetLightID())
		, m_data(fr::ShadowMapComponent::CreateRenderData(shadowMapCmpt))
	{
	}


	void UpdateShadowMapDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		UpdateShadowMapDataRenderCommand* cmdPtr = reinterpret_cast<UpdateShadowMapDataRenderCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			/*gr::RenderDataManager& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();*/

				// TODO: SET THE DATA WITH GS'S, RENDER DATA MGR, ETC
		}
	}


	void UpdateShadowMapDataRenderCommand::Destroy(void* cmdData)
	{
		UpdateShadowMapDataRenderCommand* cmdPtr = reinterpret_cast<UpdateShadowMapDataRenderCommand*>(cmdData);
		cmdPtr->~UpdateShadowMapDataRenderCommand();
	}
}