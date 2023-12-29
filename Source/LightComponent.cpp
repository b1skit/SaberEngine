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
	std::atomic<uint32_t> LightComponent::s_lightIDs = 0;


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
			fr::Light::LightType::Point_Deferred,
			colorIntensity,
			hasShadow);
		em.EmplaceComponent<PointDeferredMarker>(lightEntity);

		// ShadowMap, if required:
		if (hasShadow)
		{
			fr::ShadowMapComponent::AttachShadowMapComponent(
				em, lightEntity, std::format("{}_ShadowMap", name).c_str(), fr::Light::LightType::Point_Deferred);
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
			fr::Light::LightType::Directional_Deferred,
			colorIntensity,
			hasShadow);
		em.EmplaceComponent<DirectionalDeferredMarker>(lightEntity);

		// ShadowMap, if required:
		if (hasShadow)
		{
			fr::ShadowMapComponent::AttachShadowMapComponent(
				em, lightEntity, std::format("{}_ShadowMap", name).c_str(), fr::Light::LightType::Directional_Deferred);
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


	gr::Light::RenderData LightComponent::CreateRenderData(
		fr::NameComponent const& nameCmpt, fr::LightComponent const& lightCmpt)
	{
		gr::Light::RenderData renderData(
			nameCmpt.GetName().c_str(),
			fr::Light::ConvertRenderDataLightType(lightCmpt.m_light.GetType()),
			lightCmpt.GetLightID(),
			lightCmpt.GetRenderDataID(),
			lightCmpt.GetTransformID(),
			lightCmpt.m_hasShadow);

		fr::Light const& light = lightCmpt.m_light;

		switch (light.GetType())
		{
		case fr::Light::LightType::AmbientIBL_Deferred:
		{
			fr::Light::TypeProperties const& typeProperties =
				light.GetLightTypeProperties(fr::Light::LightType::AmbientIBL_Deferred);
			SEAssert("IBL texture cannot be null", typeProperties.m_ambient.m_IBLTex);

			renderData.m_typeProperties.m_ambient.m_iblTex = typeProperties.m_ambient.m_IBLTex;

			renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
			renderData.m_specularEnabled = typeProperties.m_specularEnabled;
		}
		break;
		case fr::Light::LightType::Directional_Deferred:
		{
			fr::Light::TypeProperties const& typeProperties = 
				light.GetLightTypeProperties(fr::Light::LightType::Directional_Deferred);

			renderData.m_typeProperties.m_directional.m_colorIntensity = typeProperties.m_directional.m_colorIntensity;

			renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
			renderData.m_specularEnabled = typeProperties.m_specularEnabled;
			
		}
		break;
		case fr::Light::LightType::Point_Deferred:
		{
			fr::Light::TypeProperties const& typeProperties =
				light.GetLightTypeProperties(fr::Light::LightType::Point_Deferred);

			renderData.m_typeProperties.m_point.m_colorIntensity = typeProperties.m_point.m_colorIntensity;
			renderData.m_typeProperties.m_point.m_emitterRadius = typeProperties.m_point.m_emitterRadius;
			renderData.m_typeProperties.m_point.m_intensityCuttoff = typeProperties.m_point.m_intensityCuttoff;
			renderData.m_typeProperties.m_point.m_sphericalRadius = typeProperties.m_point.m_sphericalRadius;

			renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
			renderData.m_specularEnabled = typeProperties.m_specularEnabled;
		}
		break;
		default: SEAssertF("Invalid light type");
		}
		
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
			case fr::Light::LightType::AmbientIBL_Deferred:
			{
				//
			}
			break;
			case fr::Light::LightType::Directional_Deferred:
			{
				//
			}
			break;
			case fr::Light::LightType::Point_Deferred:
			{
				SEAssert("Point lights require a Transform", lightTransform);
				SEAssert("Light is not a point light", light.GetType() == fr::Light::LightType::Point_Deferred);

				fr::Light::TypeProperties& lightProperties =
					light.GetLightTypePropertiesForModification(fr::Light::LightType::Point_Deferred);

				// Scale the owning transform such that a sphere created with a radius of 1 will be the correct size
				lightTransform->SetLocalScale(glm::vec3(lightProperties.m_point.m_sphericalRadius));
			}
			break;
			default: SEAssertF("Invalid light type");
			}
		}

		return didModify;
	}


	// ---


	LightComponent::LightComponent(
		PrivateCTORTag,
		gr::RenderDataComponent const& renderDataComponent, 
		fr::Light::LightType lightType, 
		glm::vec4 colorIntensity,
		bool hasShadow)
		: m_lightID(s_lightIDs.fetch_add(1))
		, m_renderDataID(renderDataComponent.GetRenderDataID())
		, m_transformID(renderDataComponent.GetTransformID())
		, m_light(lightType, colorIntensity)
		, m_hasShadow(hasShadow)
	{
	}


	LightComponent::LightComponent(
		PrivateCTORTag, 
		gr::RenderDataComponent const& renderDataComponent,
		re::Texture const* iblTex,
		const fr::Light::LightType ambientTypeOnly)
		: m_lightID(s_lightIDs.fetch_add(1))
		, m_renderDataID(renderDataComponent.GetRenderDataID())
		, m_transformID(renderDataComponent.GetTransformID())
		, m_light(iblTex, fr::Light::LightType::AmbientIBL_Deferred)
		, m_hasShadow(false)
	{
		SEAssert("This constructor is for ambient light types only", 
			ambientTypeOnly == fr::Light::LightType::AmbientIBL_Deferred);
	}


	// ---


	UpdateLightDataRenderCommand::UpdateLightDataRenderCommand(
		fr::NameComponent const& nameComponent, LightComponent const& lightComponent)
		: m_lightID(lightComponent.GetLightID())
		, m_renderDataID(lightComponent.GetRenderDataID())
		, m_transformID(lightComponent.GetTransformID())
		, m_data(fr::LightComponent::CreateRenderData(nameComponent, lightComponent))
	{
	}


	void UpdateLightDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		UpdateLightDataRenderCommand* cmdPtr = reinterpret_cast<UpdateLightDataRenderCommand*>(cmdData);

		for (size_t rsIdx = 0; rsIdx < renderSystems.size(); rsIdx++)
		{
			gr::DeferredLightingGraphicsSystem* deferredLightGS = 
				renderSystems[rsIdx]->GetGraphicsSystemManager().GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>();

			if (deferredLightGS)
			{
				gr::Light::RenderData const& lightRenderData = cmdPtr->m_data;

				std::vector<gr::Light::RenderData>* gsRenderData = nullptr;

				switch (lightRenderData.m_lightType)
				{
				case gr::Light::LightType::AmbientIBL_Deferred:
				{
					gsRenderData = &deferredLightGS->GetRenderData(gr::Light::LightType::AmbientIBL_Deferred);
				}
				break;
				case gr::Light::LightType::Directional_Deferred:
				{
					gsRenderData = &deferredLightGS->GetRenderData(gr::Light::LightType::Directional_Deferred);
				}
				break;
				case gr::Light::LightType::Point_Deferred:
				{
					gsRenderData = &deferredLightGS->GetRenderData(gr::Light::LightType::Point_Deferred);
				}
				break;
				default: SEAssertF("Invalid light type");
				}

				auto existingLightItr = std::find_if(gsRenderData->begin(), gsRenderData->end(),
					[&](gr::Light::RenderData const& existingLight)
					{
						return lightRenderData.m_lightID == existingLight.m_lightID;
					});

				if (existingLightItr == gsRenderData->end()) // New light
				{
					gsRenderData->emplace_back(lightRenderData);
				}
				else
				{
					*existingLightItr = lightRenderData;
				}
			}
		}
	}


	void UpdateLightDataRenderCommand::Destroy(void* cmdData)
	{
		UpdateLightDataRenderCommand* cmdPtr = reinterpret_cast<UpdateLightDataRenderCommand*>(cmdData);
		cmdPtr->~UpdateLightDataRenderCommand();
	}
}