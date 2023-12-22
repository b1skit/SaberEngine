// © 2023 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "MeshFactory.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "RenderSystem.h"
#include "ShadowMapComponent.h"
#include "TransformComponent.h"


namespace fr
{
	std::atomic<uint32_t> LightComponent::s_lightIDs = 0;


	LightComponent& LightComponent::CreateDeferredAmbientLightConcept(char const* name)
	{
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		entt::entity lightEntity = gpm.CreateEntity(name);

		// MeshPrimitive:
		gr::RenderDataComponent& renderDataComponent = 
			gr::RenderDataComponent::AttachNewRenderDataComponent(gpm, lightEntity, gr::k_sharedIdentityTransformID);

		std::shared_ptr<gr::MeshPrimitive> fullscreenQuadSceneData = 
			gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Far);

		fr::MeshPrimitive::MeshPrimitiveComponent const& meshPrimCmpt = fr::MeshPrimitive::AttachRawMeshPrimitiveConcept(
			gpm,
			lightEntity,
			renderDataComponent,
			fullscreenQuadSceneData.get());

		// LightComponent:
		fr::LightComponent& lightComponent = *gpm.EmplaceComponent<fr::LightComponent>(
			lightEntity, 
			PrivateCTORTag{}, 
			renderDataComponent,
			fr::Light::LightType::AmbientIBL_Deferred);

		// Mark our new LightComponent as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::LightComponent>>(lightEntity);

		return lightComponent;
	}


	LightComponent& LightComponent::AttachDeferredPointLightConcept(
		entt::entity owningEntity, char const* name, glm::vec4 colorIntensity, bool hasShadow)
	{
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		SEAssert("A light's owning entity requires a Relationship component",
			gpm.HasComponent<fr::Relationship>(owningEntity));
		SEAssert("A light's owning entity requires a TransformComponent",
			fr::Relationship::IsInHierarchyAbove<fr::TransformComponent>(owningEntity));

		entt::entity lightEntity = gpm.CreateEntity(name);

		// Relationship:
		fr::Relationship& lightRelationship = fr::Relationship::AttachRelationshipComponent(gpm, lightEntity);
		lightRelationship.SetParent(gpm, owningEntity);

		// MeshPrimitive:
		std::shared_ptr<gr::MeshPrimitive> pointLightMesh = gr::meshfactory::CreateSphere();

		entt::entity meshPrimitiveEntity = 
			fr::MeshPrimitive::AttachMeshPrimitiveConcept(lightEntity, pointLightMesh.get());
		gr::RenderDataComponent const& meshPrimRenderDataCmpt = 
			gpm.GetComponent<gr::RenderDataComponent>(meshPrimitiveEntity);

		// RenderData: We share the deferred light MeshPrimitive's RenderDataID
		gr::RenderDataComponent::AttachSharedRenderDataComponent(gpm, lightEntity, meshPrimRenderDataCmpt);

		// LightComponent:
		fr::LightComponent& lightComponent = *gpm.EmplaceComponent<fr::LightComponent>(
			lightEntity, 
			PrivateCTORTag{}, 
			meshPrimRenderDataCmpt,
			fr::Light::LightType::Point_Deferred,
			colorIntensity);
		
		// ShadowMap, if required:
		if (hasShadow)
		{
			fr::ShadowMapComponent::AttachShadowMapComponent(
				gpm, lightEntity, std::format("{}_ShadowMap", name).c_str(), fr::Light::LightType::Point_Deferred);
		}

		// Mark our new LightComponent as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::LightComponent>>(lightEntity);

		return lightComponent;
	}


	LightComponent& LightComponent::AttachDeferredDirectionalLightConcept(
		entt::entity owningEntity, char const* name, glm::vec4 colorIntensity, bool hasShadow)
	{
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		SEAssert("A light's owning entity requires a Relationship component",
			gpm.HasComponent<fr::Relationship>(owningEntity));
		SEAssert("A light's owning entity requires a TransformComponent",
			fr::Relationship::IsInHierarchyAbove<fr::TransformComponent>(owningEntity));

		entt::entity lightEntity = gpm.CreateEntity(name);

		// Relationship:
		fr::Relationship& lightRelationship = fr::Relationship::AttachRelationshipComponent(gpm, lightEntity);
		lightRelationship.SetParent(gpm, owningEntity);

		// MeshPrimitive:
		gr::RenderDataComponent& renderDataComponent =
			gr::RenderDataComponent::AttachNewRenderDataComponent(gpm, lightEntity, gr::k_sharedIdentityTransformID);

		std::shared_ptr<gr::MeshPrimitive> fullscreenQuadSceneData =
			gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Far);

		fr::MeshPrimitive::MeshPrimitiveComponent const& meshPrimCmpt = fr::MeshPrimitive::AttachRawMeshPrimitiveConcept(
			gpm,
			lightEntity,
			renderDataComponent,
			fullscreenQuadSceneData.get());

		// LightComponent:
		fr::LightComponent& lightComponent = *gpm.EmplaceComponent<fr::LightComponent>(
			lightEntity,
			PrivateCTORTag{},
			renderDataComponent,
			fr::Light::LightType::Point_Deferred,
			colorIntensity);

		// ShadowMap, if required:
		if (hasShadow)
		{
			fr::ShadowMapComponent::AttachShadowMapComponent(
				gpm, lightEntity, std::format("{}_ShadowMap", name).c_str(), fr::Light::LightType::Directional_Deferred);
		}

		// Mark our new LightComponent as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::LightComponent>>(lightEntity);

		return lightComponent;
	}


	gr::Light::RenderData LightComponent::CreateRenderData(fr::LightComponent const& lightCmpt)
	{
		SEAssertF("TODO");
		// TODO....

		return gr::Light::RenderData{};
	}


	// ---


	LightComponent::LightComponent(
		PrivateCTORTag,
		gr::RenderDataComponent const& renderDataComponent, 
		fr::Light::LightType lightType, 
		glm::vec4 colorIntensity)
		: m_lightID(s_lightIDs.fetch_add(1))
		, m_renderDataID(renderDataComponent.GetRenderDataID())
		, m_transformID(renderDataComponent.GetTransformID())
		, m_light(
			"NAME IS DEPRECATED!!!!!!!!!!!!!!!!!!!!!!",
			nullptr, // owningTransform IS DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			lightType,
			colorIntensity,
			true) // hasShadow IS DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	{
	}


	LightComponent::LightComponent(
		PrivateCTORTag, 
		gr::RenderDataComponent const& renderDataComponent, 
		fr::Light::LightType ambientTypeOnly)
		: m_lightID(s_lightIDs.fetch_add(1))
		, m_renderDataID(renderDataComponent.GetRenderDataID())
		, m_transformID(renderDataComponent.GetTransformID())
		, m_light(
			"NAME IS DEPRECATED!!!!!!!!!!!!!!!!!!!!!!",
			nullptr, // owningTransform IS DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			fr::Light::LightType::AmbientIBL_Deferred,
			glm::vec4(1.f, 0.f, 1.f, 1.f), // Magenta: Color/instensity is not used for ambient lights
			false) // hasShadow IS DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	{
		SEAssert("This constructor is for ambient light types only", 
			ambientTypeOnly == fr::Light::LightType::AmbientIBL_Deferred);
	}


	// ---


	UpdateLightDataRenderCommand::UpdateLightDataRenderCommand(LightComponent const& lightComponent)
		: m_lightID(lightComponent.GetLightID())
		, m_renderDataID(lightComponent.GetRenderDataID())
		, m_transformID(lightComponent.GetTransformID())
		, m_data(fr::LightComponent::CreateRenderData(lightComponent))
	{
	}


	void UpdateLightDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		UpdateLightDataRenderCommand* cmdPtr = reinterpret_cast<UpdateLightDataRenderCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			/*gr::RenderDataManager& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();*/

			// TODO: SET THE DATA WITH GS'S, RENDER DATA MGR, ETC
		}
	}


	void UpdateLightDataRenderCommand::Destroy(void* cmdData)
	{
		UpdateLightDataRenderCommand* cmdPtr = reinterpret_cast<UpdateLightDataRenderCommand*>(cmdData);
		cmdPtr->~UpdateLightDataRenderCommand();
	}
}