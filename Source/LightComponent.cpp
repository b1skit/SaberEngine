// © 2023 Adam Badke. All rights reserved.
#include "GraphicsSystem_DeferredLighting.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "MeshFactory.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "RenderSystem.h"
#include "SceneManager.h"
#include "ShadowMapComponent.h"
#include "TransformComponent.h"


namespace fr
{
	LightComponent& LightComponent::CreateDeferredAmbientLightConcept(EntityManager& em, re::Texture const* iblTex)
	{
		SEAssert("IBL texture cannot be null", iblTex);

		entt::entity lightEntity = em.CreateEntity(iblTex->GetName());

		// MeshPrimitive:
		gr::RenderDataComponent& renderDataComponent = 
			gr::RenderDataComponent::AttachNewRenderDataComponent(em, lightEntity, gr::k_sharedIdentityTransformID);

		std::shared_ptr<gr::MeshPrimitive> fullscreenQuadSceneData = 
			gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Far);

		fr::MeshPrimitiveComponent const& meshPrimCmpt = fr::MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
			em,
			lightEntity,
			renderDataComponent,
			fullscreenQuadSceneData.get());

		// LightComponent:
		fr::LightComponent& lightComponent = *em.EmplaceComponent<fr::LightComponent>(
			lightEntity, 
			PrivateCTORTag{}, 
			renderDataComponent,
			iblTex);
		em.EmplaceComponent<AmbientIBLDeferredMarker>(lightEntity);

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<fr::LightComponent>>(lightEntity);

		return lightComponent;
	}


	LightComponent& LightComponent::AttachDeferredPointLightConcept(
		fr::EntityManager& em,
		entt::entity owningEntity, 
		char const* name, 
		glm::vec4 const& colorIntensity, 
		bool hasShadow)
	{
		SEAssert("A light's owning entity requires a TransformComponent",
			em.IsInHierarchyAbove<fr::TransformComponent>(owningEntity));

		entt::entity lightEntity = em.CreateEntity(name);

		// Relationship:
		fr::Relationship& lightRelationship = em.GetComponent<fr::Relationship>(lightEntity);
		lightRelationship.SetParent(em, owningEntity);

		// MeshPrimitive:
		std::shared_ptr<gr::MeshPrimitive> pointLightMesh = gr::meshfactory::CreateSphere();

		entt::entity meshPrimitiveEntity =
			fr::MeshPrimitiveComponent::AttachMeshPrimitiveConcept(em, lightEntity, pointLightMesh.get());

		// RenderData:
		// Deferred point light's share the the RenderDataComponent created by their MeshPrimitive sphere
		gr::RenderDataComponent const& meshPrimRenderDataCmpt = 
			em.GetComponent<gr::RenderDataComponent>(meshPrimitiveEntity);
		gr::RenderDataComponent::AttachSharedRenderDataComponent(em, lightEntity, meshPrimRenderDataCmpt);

		// LightComponent:
		fr::LightComponent& lightComponent = *em.EmplaceComponent<fr::LightComponent>(
			lightEntity, 
			PrivateCTORTag{}, 
			meshPrimRenderDataCmpt,
			fr::Light::Type::Point,
			colorIntensity,
			hasShadow);
		em.EmplaceComponent<PointDeferredMarker>(lightEntity);

		// ShadowMap, if required:
		if (hasShadow)
		{
			fr::ShadowMapComponent::AttachShadowMapComponent(
				em, lightEntity, std::format("{}_ShadowMap", name).c_str(), fr::Light::Type::Point);
			em.EmplaceComponent<HasShadowMarker>(lightEntity);
		}

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<fr::LightComponent>>(lightEntity);

		return lightComponent;
	}


	LightComponent& LightComponent::AttachDeferredPointLightConcept(
		fr::EntityManager& em, 
		entt::entity entity, 
		std::string const& name, 
		glm::vec4 const& colorIntensity, 
		bool hasShadow)
	{
		return AttachDeferredPointLightConcept(em, entity, name.c_str(), colorIntensity, hasShadow);
	}


	LightComponent& LightComponent::AttachDeferredDirectionalLightConcept(
		fr::EntityManager& em,
		entt::entity owningEntity, 
		char const* name, 
		glm::vec4 colorIntensity, 
		bool hasShadow)
	{
		SEAssert("A light's owning entity requires a TransformComponent",
			em.IsInHierarchyAbove<fr::TransformComponent>(owningEntity));

		entt::entity lightEntity = em.CreateEntity(name);

		// Relationship:
		fr::Relationship& lightRelationship = em.GetComponent<fr::Relationship>(lightEntity);
		lightRelationship.SetParent(em, owningEntity);

		fr::TransformComponent const& transformComponent = 
			*em.GetFirstInHierarchyAbove<fr::TransformComponent>(owningEntity);

		// Note: A light requires a RenderDataComponent and Transform, and a raw MeshPrimitiveComponent requires a
		// RenderDataComponent to share. This means our fullscreen quad will technically be linked to the light's
		// Transform; Fullscreen quads don't use a a Transform so this shouldn't matter
		gr::RenderDataComponent& renderDataComponent =
			gr::RenderDataComponent::AttachNewRenderDataComponent(em, lightEntity, transformComponent.GetTransformID());

		// MeshPrimitive:
		std::shared_ptr<gr::MeshPrimitive> fullscreenQuadSceneData =
			gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Far);

		fr::MeshPrimitiveComponent const& meshPrimCmpt = fr::MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
			em,
			lightEntity,
			renderDataComponent,
			fullscreenQuadSceneData.get());

		// LightComponent:
		LightComponent& lightComponent = *em.EmplaceComponent<LightComponent>(
			lightEntity,
			PrivateCTORTag{},
			renderDataComponent,
			fr::Light::Type::Directional,
			colorIntensity,
			hasShadow);
		em.EmplaceComponent<DirectionalDeferredMarker>(lightEntity);

		// ShadowMap, if required:
		if (hasShadow)
		{
			fr::ShadowMapComponent::AttachShadowMapComponent(
				em, lightEntity, std::format("{}_ShadowMap", name).c_str(), fr::Light::Type::Directional);
			em.EmplaceComponent<HasShadowMarker>(lightEntity);
		}

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<fr::LightComponent>>(lightEntity);

		return lightComponent;
	}


	LightComponent& LightComponent::AttachDeferredDirectionalLightConcept(
		fr::EntityManager& em,
		entt::entity entity, 
		std::string const& name, 
		glm::vec4 colorIntensity, 
		bool hasShadow)
	{
		return AttachDeferredDirectionalLightConcept(em, entity, name.c_str(), colorIntensity, hasShadow);
	}


	gr::Light::RenderDataAmbientIBL LightComponent::CreateRenderDataAmbientIBL_Deferred(
		fr::NameComponent const& nameCmpt, fr::LightComponent const& lightCmpt)
	{
		gr::Light::RenderDataAmbientIBL renderData(
			nameCmpt.GetName().c_str(),
			lightCmpt.GetRenderDataID(),
			lightCmpt.GetTransformID());

		fr::Light const& light = lightCmpt.m_light;

		fr::Light::TypeProperties const& typeProperties =
			light.GetLightTypeProperties(fr::Light::Type::AmbientIBL);
		SEAssert("IBL texture cannot be null", typeProperties.m_ambient.m_IBLTex);

		renderData.m_iblTex = typeProperties.m_ambient.m_IBLTex;

		renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
		renderData.m_specularEnabled = typeProperties.m_specularEnabled;

		return renderData;
	}

	gr::Light::RenderDataDirectional LightComponent::CreateRenderDataDirectional_Deferred(
		fr::NameComponent const& nameCmpt, fr::LightComponent const& lightCmpt)
	{
		gr::Light::RenderDataDirectional renderData(
			nameCmpt.GetName().c_str(),
			lightCmpt.GetRenderDataID(),
			lightCmpt.GetTransformID());

		fr::Light const& light = lightCmpt.m_light;

		fr::Light::TypeProperties const& typeProperties =
			light.GetLightTypeProperties(fr::Light::Type::Directional);

		renderData.m_colorIntensity = typeProperties.m_directional.m_colorIntensity;

		renderData.m_hasShadow = lightCmpt.m_hasShadow;

		renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
		renderData.m_specularEnabled = typeProperties.m_specularEnabled;

		return renderData;
	}


	gr::Light::RenderDataPoint LightComponent::CreateRenderDataPoint_Deferred(
		fr::NameComponent const& nameCmpt, fr::LightComponent const& lightCmpt)
	{
		gr::Light::RenderDataPoint renderData(
			nameCmpt.GetName().c_str(),
			lightCmpt.GetRenderDataID(),
			lightCmpt.GetTransformID());

		fr::Light const& light = lightCmpt.m_light;

		fr::Light::TypeProperties const& typeProperties =
			light.GetLightTypeProperties(fr::Light::Type::Point);

		renderData.m_colorIntensity = typeProperties.m_point.m_colorIntensity;
		renderData.m_emitterRadius = typeProperties.m_point.m_emitterRadius;
		renderData.m_intensityCuttoff = typeProperties.m_point.m_intensityCuttoff;

		renderData.m_sphericalRadius = typeProperties.m_point.m_sphericalRadius;

		renderData.m_hasShadow = lightCmpt.m_hasShadow;

		renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
		renderData.m_specularEnabled = typeProperties.m_specularEnabled;

		return renderData;
	}


	bool LightComponent::Update(fr::LightComponent& lightComponent, fr::Transform* lightTransform, fr::Camera* shadowCam)
	{
		fr::Light& light = lightComponent.GetLight();

		bool didModify = light.Update();

		if (didModify)
		{
			switch (light.GetType())
			{
			case fr::Light::Type::AmbientIBL:
			{
				//
			}
			break;
			case fr::Light::Type::Directional:
			{
				//
			}
			break;
			case fr::Light::Type::Point:
			{
				SEAssert("Point lights require a Transform", lightTransform);
				SEAssert("Light is not a point light", light.GetType() == fr::Light::Type::Point);

				fr::Light::TypeProperties& lightProperties =
					light.GetLightTypePropertiesForModification(fr::Light::Type::Point);

				// Scale the owning transform such that a sphere created with a radius of 1 will be the correct size
				lightTransform->SetLocalScale(glm::vec3(lightProperties.m_point.m_sphericalRadius));
			}
			break;
			default: SEAssertF("Invalid light type");
			}
		}

		return didModify;
	}


	void LightComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity lightEntity)
	{
		fr::NameComponent const& nameCmpt = em.GetComponent<fr::NameComponent>(lightEntity);

		if (ImGui::CollapsingHeader(
			std::format("{}##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, lightEntity);

			fr::LightComponent& lightCmpt = em.GetComponent<fr::LightComponent>(lightEntity);
			
			lightCmpt.GetLight().ShowImGuiWindow(nameCmpt.GetUniqueID());

			// Transform:
			entt::entity transformOwningEntity = entt::null;
			fr::TransformComponent* transformComponent =
				em.GetFirstAndEntityInHierarchyAbove<fr::TransformComponent>(lightEntity, transformOwningEntity);
			SEAssert("Failed to find TransformComponent", 
				transformComponent || lightCmpt.m_light.GetType() == fr::Light::Type::AmbientIBL);
			if (transformComponent)
			{
				fr::TransformComponent::ShowImGuiWindow(em, transformOwningEntity, static_cast<uint64_t>(lightEntity));
			}

			// Shadow map
			entt::entity shadowMapEntity = entt::null;
			fr::ShadowMapComponent* shadowMapCmpt = 
				em.GetFirstAndEntityInChildren<fr::ShadowMapComponent>(lightEntity, shadowMapEntity);
			if (shadowMapCmpt)
			{
				fr::ShadowMapComponent::ShowImGuiWindow(em, shadowMapEntity);
			}

			ImGui::Unindent();
		}
	}


	// ---


	LightComponent::LightComponent(
		PrivateCTORTag,
		gr::RenderDataComponent const& renderDataComponent, 
		fr::Light::Type lightType, 
		glm::vec4 colorIntensity,
		bool hasShadow)
		: m_renderDataID(renderDataComponent.GetRenderDataID())
		, m_transformID(renderDataComponent.GetTransformID())
		, m_light(lightType, colorIntensity)
		, m_hasShadow(hasShadow)
	{
	}


	LightComponent::LightComponent(
		PrivateCTORTag, 
		gr::RenderDataComponent const& renderDataComponent,
		re::Texture const* iblTex,
		const fr::Light::Type ambientTypeOnly)
		: m_renderDataID(renderDataComponent.GetRenderDataID())
		, m_transformID(renderDataComponent.GetTransformID())
		, m_light(iblTex, fr::Light::Type::AmbientIBL)
		, m_hasShadow(false)
	{
		SEAssert("This constructor is for ambient light types only", 
			ambientTypeOnly == fr::Light::Type::AmbientIBL);
	}


	// ---


	UpdateLightDataRenderCommand::UpdateLightDataRenderCommand(
		fr::NameComponent const& nameComponent, LightComponent const& lightComponent)
		: m_renderDataID(lightComponent.GetRenderDataID())
		, m_transformID(lightComponent.GetTransformID())
	{
		m_type = fr::Light::ConvertRenderDataLightType(lightComponent.GetLight().GetType());
		switch (m_type)
		{
		case gr::Light::Type::AmbientIBL:
		{
			m_ambientData = fr::LightComponent::CreateRenderDataAmbientIBL_Deferred(nameComponent, lightComponent);
		}
		break;
		case gr::Light::Type::Directional:
		{
			m_directionalData = fr::LightComponent::CreateRenderDataDirectional_Deferred(nameComponent, lightComponent);
		}
		break;
		case gr::Light::Type::Point:
		{
			m_pointData = fr::LightComponent::CreateRenderDataPoint_Deferred(nameComponent, lightComponent);
		}
		break;
		default: SEAssertF("Invalid type");
		}
	}


	void UpdateLightDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		UpdateLightDataRenderCommand* cmdPtr = reinterpret_cast<UpdateLightDataRenderCommand*>(cmdData);

		for (size_t rsIdx = 0; rsIdx < renderSystems.size(); rsIdx++)
		{
			gr::GraphicsSystemManager& gsm = renderSystems[rsIdx]->GetGraphicsSystemManager();

			gr::RenderDataManager& renderDataMgr = gsm.GetRenderDataForModification();
			
			switch (cmdPtr->m_type)
			{
			case gr::Light::Type::AmbientIBL:
			{
				renderDataMgr.SetObjectData<gr::Light::RenderDataAmbientIBL>(
					cmdPtr->m_renderDataID, &cmdPtr->m_ambientData);
			}
			break;
			case gr::Light::Type::Directional:
			{
				renderDataMgr.SetObjectData<gr::Light::RenderDataDirectional>(
					cmdPtr->m_renderDataID, &cmdPtr->m_directionalData);
			}
			break;
			case gr::Light::Type::Point:
			{
				renderDataMgr.SetObjectData<gr::Light::RenderDataPoint>(
					cmdPtr->m_renderDataID, &cmdPtr->m_pointData);
			}
			break;
			default: SEAssertF("Invalid type");
			}

			// Register the light with the deferred lighting GS:
			gr::DeferredLightingGraphicsSystem* deferredLightGS = 
				renderSystems[rsIdx]->GetGraphicsSystemManager().GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>();

			if (deferredLightGS)
			{
				deferredLightGS->RegisterLight(cmdPtr->m_type, cmdPtr->m_renderDataID);
			}
		}
	}


	void UpdateLightDataRenderCommand::Destroy(void* cmdData)
	{
		UpdateLightDataRenderCommand* cmdPtr = reinterpret_cast<UpdateLightDataRenderCommand*>(cmdData);
		cmdPtr->~UpdateLightDataRenderCommand();
	}


	// ---


	DestroyLightDataRenderCommand::DestroyLightDataRenderCommand(LightComponent const& lightCmpt)
		: m_renderDataID(lightCmpt.GetRenderDataID())
		, m_type(fr::Light::ConvertRenderDataLightType(lightCmpt.GetLight().GetType()))
	{
	}


	void DestroyLightDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		DestroyLightDataRenderCommand* cmdPtr = reinterpret_cast<DestroyLightDataRenderCommand*>(cmdData);

		for (size_t rsIdx = 0; rsIdx < renderSystems.size(); rsIdx++)
		{
			gr::GraphicsSystemManager& gsm = renderSystems[rsIdx]->GetGraphicsSystemManager();

			gr::RenderDataManager& renderDataMgr = gsm.GetRenderDataForModification();

			switch (cmdPtr->m_type)
			{
			case fr::Light::Type::AmbientIBL:
			{
				renderDataMgr.DestroyObjectData<gr::Light::RenderDataAmbientIBL>(cmdPtr->m_renderDataID);
			}
			break;
			case fr::Light::Type::Directional:
			{
				renderDataMgr.DestroyObjectData<gr::Light::RenderDataDirectional>(cmdPtr->m_renderDataID);
			}
			break;
			case fr::Light::Type::Point:
			{
				renderDataMgr.DestroyObjectData<gr::Light::RenderDataPoint>(cmdPtr->m_renderDataID);
			}
			break;
			default: SEAssertF("Invalid type");
			}

			// Unregister the light from the deferred lighting GS:
			gr::DeferredLightingGraphicsSystem* deferredLightGS =
				renderSystems[rsIdx]->GetGraphicsSystemManager().GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>();
			
			if (deferredLightGS)
			{
				deferredLightGS->UnregisterLight(cmdPtr->m_type, cmdPtr->m_renderDataID);
			}
		}
	}


	void DestroyLightDataRenderCommand::Destroy(void* cmdData)
	{
		DestroyLightDataRenderCommand* cmdPtr = reinterpret_cast<DestroyLightDataRenderCommand*>(cmdData);
		cmdPtr->~DestroyLightDataRenderCommand();
	}
}