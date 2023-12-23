// © 2023 Adam Badke. All rights reserved.
#include "GraphicsSystem_DeferredLighting.h"
#include "GameplayManager.h"
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


	LightComponent& LightComponent::CreateDeferredAmbientLightConcept(re::Texture const* iblTex)
	{
		SEAssert("IBL texture cannot be null", iblTex);

		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		entt::entity lightEntity = gpm.CreateEntity(iblTex->GetName());

		// Relationship:
		fr::Relationship& ambientLightRelationship = fr::Relationship::AttachRelationshipComponent(gpm, lightEntity);

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
			iblTex);

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
		gr::Light::RenderData renderData(
			fr::Light::GetRenderDataLightType(lightCmpt.m_light.GetType()),
			lightCmpt.GetLightID(),
			lightCmpt.GetRenderDataID(),
			lightCmpt.GetTransformID());

		fr::Light const& light = lightCmpt.m_light;

		switch (light.GetType())
		{
		case fr::Light::LightType::AmbientIBL_Deferred:
		{
			fr::Light::TypeProperties const& typeProperties =
				light.AccessLightTypeProperties(fr::Light::LightType::AmbientIBL_Deferred);
			SEAssert("IBL texture cannot be null", typeProperties.m_ambient.m_IBLTex);

			renderData.m_typeProperties.m_ambient.m_iblTex = typeProperties.m_ambient.m_IBLTex;

			renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
			renderData.m_specularEnabled = typeProperties.m_specularEnabled;
		}
		break;
		case fr::Light::LightType::Directional_Deferred:
		{
			fr::Light::TypeProperties const& typeProperties = 
				light.AccessLightTypeProperties(fr::Light::LightType::Directional_Deferred);

			renderData.m_typeProperties.m_directional.m_colorIntensity = typeProperties.m_directional.m_colorIntensity;

			renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
			renderData.m_specularEnabled = typeProperties.m_specularEnabled;
			
		}
		break;
		case fr::Light::LightType::Point_Deferred:
		{
			fr::Light::TypeProperties const& typeProperties =
				light.AccessLightTypeProperties(fr::Light::LightType::Point_Deferred);

			renderData.m_typeProperties.m_point.m_colorIntensity = typeProperties.m_point.m_colorIntensity;
			renderData.m_typeProperties.m_point.m_emitterRadius = typeProperties.m_point.m_emitterRadius;
			renderData.m_typeProperties.m_point.m_intensityCuttoff = typeProperties.m_point.m_intensityCuttoff;

			renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
			renderData.m_specularEnabled = typeProperties.m_specularEnabled;
		}
		break;
		default: SEAssertF("Invalid light type");
		}
		
		return renderData;
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
		re::Texture const* iblTex,
		const fr::Light::LightType ambientTypeOnly)
		: m_lightID(s_lightIDs.fetch_add(1))
		, m_renderDataID(renderDataComponent.GetRenderDataID())
		, m_transformID(renderDataComponent.GetTransformID())
		, m_light(iblTex, fr::Light::LightType::AmbientIBL_Deferred)
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
			gr::DeferredLightingGraphicsSystem* deferredLightGS = 
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetGraphicsSystem<gr::DeferredLightingGraphicsSystem>();

			if (deferredLightGS)
			{
				gr::Light::RenderData const& renderData = cmdPtr->m_data;
				switch (renderData.m_lightType)
				{
				case gr::Light::LightType::AmbientIBL_Deferred:
				{
					std::vector<gr::Light::RenderData>& ambientRenderData =
						deferredLightGS->GetRenderData(gr::Light::LightType::AmbientIBL_Deferred);

					auto existingAmbientItr = std::find_if(ambientRenderData.begin(), ambientRenderData.end(),
						[&](gr::Light::RenderData const& existingLight)
						{
							return renderData.m_lightID == existingLight.m_lightID;
						});

					if (existingAmbientItr == ambientRenderData.end()) // New light
					{
						ambientRenderData.emplace_back(renderData);
					}
					else
					{
						*existingAmbientItr = renderData;
					}
				}
				break;
				case gr::Light::LightType::Directional_Deferred:
				{
					std::vector<gr::Light::RenderData>& directionalRenderData =
						deferredLightGS->GetRenderData(gr::Light::LightType::Directional_Deferred);

					auto existingDirectionalItr = std::find_if(directionalRenderData.begin(), directionalRenderData.end(),
						[&](gr::Light::RenderData const& existingLight)
						{
							return renderData.m_lightID == existingLight.m_lightID;
						});

					if (existingDirectionalItr == directionalRenderData.end()) // New light
					{
						directionalRenderData.emplace_back(renderData);
					}
					else
					{
						*existingDirectionalItr = renderData;
					}
				}
				break;
				case gr::Light::LightType::Point_Deferred:
				{
					std::vector<gr::Light::RenderData>& pointRenderData =
						deferredLightGS->GetRenderData(gr::Light::LightType::Point_Deferred);

					auto existingPointItr = std::find_if(pointRenderData.begin(), pointRenderData.end(),
						[&](gr::Light::RenderData const& existingLight)
						{
							return renderData.m_lightID == existingLight.m_lightID;
						});

					if (existingPointItr == pointRenderData.end()) // New light
					{
						pointRenderData.emplace_back(renderData);
					}
					else
					{
						*existingPointItr = renderData;
					}
				}
				break;
				default: SEAssertF("Invalid light type");
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