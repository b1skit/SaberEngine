// © 2023 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "Camera.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "SceneNodeConcept.h"
#include "ShadowMapComponent.h"
#include "TransformComponent.h"

#include "Core/Definitions/EventKeys.h"

#include "Core/Host/Dialog.h"

#include "Core/Util/ImGuiUtils.h"

#include "Renderer/MeshFactory.h"
#include "Renderer/RenderSystem.h"


namespace
{
	glm::vec3 ComputeConeMeshScale(float outerConeAngle, float coneHeight)
	{
		SEAssert(coneHeight > 0.f && outerConeAngle <= glm::pi<float>() * 0.5f, "Invalid cone dimensions");

		// Prevent crazy values as outerConeAngle -> pi/2
		constexpr float k_maxOuterConeAngle = glm::pi<float>() * 0.49f;
		const float coneRadiusScale = glm::tan(std::min(outerConeAngle, k_maxOuterConeAngle)) * coneHeight;

		// Note: Our cone mesh is pre-rotated during construction to extend from the origin down the Z axis
		return glm::vec3(coneRadiusScale, coneRadiusScale, coneHeight);
	}


	bool CanLightContribute(glm::vec4 const& colorIntensity, bool diffuseEnabled, bool specEnabled)
	{
		const bool isNotBlack = (colorIntensity.r > 0.f || colorIntensity.g > 0.f || colorIntensity.b > 0.f);
		const bool hasNonZeroIntensity = colorIntensity.a > 0.f;
		const bool diffuseOrSpecEnabled = diffuseEnabled || specEnabled;
		
		return isNotBlack && hasNonZeroIntensity && diffuseOrSpecEnabled;
	}
}


namespace fr
{
	entt::entity LightComponent::CreateDeferredAmbientLightConcept(
		EntityManager& em, std::string_view name, core::InvPtr<re::Texture> const& iblTex)
	{
		SEAssert(!name.empty() && iblTex, "IBL name or texture cannot be null");

		entt::entity lightEntity = em.CreateEntity(name);

		// MeshPrimitive:
		fr::RenderDataComponent* renderDataComponent = 
			fr::RenderDataComponent::GetCreateRenderDataComponent(em, lightEntity, gr::k_invalidTransformID);

		core::InvPtr<gr::MeshPrimitive> const& fullscreenQuad = 
			gr::meshfactory::CreateFullscreenQuad(em.GetInventory(), gr::meshfactory::ZLocation::Far);

		fr::MeshPrimitiveComponent const& meshPrimCmpt = fr::MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
			em,
			lightEntity,
			*renderDataComponent,
			fullscreenQuad);

		// LightComponent:
		fr::LightComponent& lightComponent = *em.EmplaceComponent<fr::LightComponent>(
			lightEntity, 
			PrivateCTORTag{}, 
			*renderDataComponent,
			iblTex);
		em.EmplaceComponent<AmbientIBLDeferredMarker>(lightEntity);

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<fr::LightComponent>>(lightEntity);

		return lightEntity;
	}


	LightComponent& LightComponent::AttachDeferredPointLightConcept(
		fr::EntityManager& em,
		entt::entity owningEntity, 
		std::string_view name, 
		glm::vec4 const& colorIntensity, 
		bool hasShadow)
	{
		// Create a MeshPrimitive:
		glm::vec3 minPos = glm::vec3(0.f);
		glm::vec3 maxPos = glm::vec3(0.f);
		const gr::meshfactory::FactoryOptions sphereOptions{ 
			.m_inventory = em.GetInventory(),
			.m_positionMinXYZOut = &minPos, 
			.m_positionMaxXYZOut = &maxPos};

		core::InvPtr<gr::MeshPrimitive> const& pointLightMesh = 
			gr::meshfactory::CreateSphere(sphereOptions, 1.f);

		fr::Relationship const& relationship = em.GetComponent<fr::Relationship>(owningEntity);

		fr::TransformComponent const* transformCmpt = relationship.GetFirstInHierarchyAbove<fr::TransformComponent>();
		if (!transformCmpt)
		{
			transformCmpt = &fr::TransformComponent::AttachTransformComponent(em, owningEntity);
		}

		fr::RenderDataComponent* renderDataComponent =
			fr::RenderDataComponent::GetCreateRenderDataComponent(em, owningEntity, transformCmpt->GetTransformID());

		renderDataComponent->SetFeatureBit(gr::RenderObjectFeature::IsLightBounds);

		// Attach the MeshPrimitive 
		fr::MeshPrimitiveComponent::AttachMeshPrimitiveComponent(em, owningEntity, pointLightMesh, minPos, maxPos);

		// LightComponent:
		fr::LightComponent& lightComponent = *em.EmplaceComponent<fr::LightComponent>(
			owningEntity,
			PrivateCTORTag{}, 
			*renderDataComponent,
			fr::Light::Type::Point,
			colorIntensity,
			hasShadow);
		em.EmplaceComponent<PointDeferredMarker>(owningEntity);

		// ShadowMapComponent, if required:
		if (hasShadow)
		{
			fr::ShadowMapComponent::AttachShadowMapComponent(
				em, owningEntity, std::format("{}_ShadowMap", name).c_str(), fr::Light::Type::Point);
		}

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<fr::LightComponent>>(owningEntity);

		return lightComponent;
	}



	LightComponent& LightComponent::AttachDeferredSpotLightConcept(
		fr::EntityManager& em,
		entt::entity owningEntity,
		std::string_view name,
		glm::vec4 const& colorIntensity,
		bool hasShadow)
	{
		// Create a MeshPrimitive:
		glm::vec3 minPos(0.f);
		glm::vec3 maxPos(0.f);

		const gr::meshfactory::FactoryOptions coneFactoryOptions{ 
			.m_inventory = em.GetInventory(),
			.m_orientation = gr::meshfactory::Orientation::ZNegative,
			.m_positionMinXYZOut = &minPos, 
			.m_positionMaxXYZOut = &maxPos
		};

		core::InvPtr<gr::MeshPrimitive> const& spotLightMesh = gr::meshfactory::CreateCone(
			coneFactoryOptions,
			1.f,	// Height
			1.f,	// Radius
			16);	// No. sides

		fr::Relationship const& relationship = em.GetComponent<fr::Relationship>(owningEntity);

		fr::TransformComponent const* transformCmpt = relationship.GetFirstInHierarchyAbove<fr::TransformComponent>();
		if (!transformCmpt)
		{
			transformCmpt = &fr::TransformComponent::AttachTransformComponent(em, owningEntity);
		}

		fr::RenderDataComponent* renderDataComponent =
			fr::RenderDataComponent::GetCreateRenderDataComponent(em, owningEntity, transformCmpt->GetTransformID());

		renderDataComponent->SetFeatureBit(gr::RenderObjectFeature::IsLightBounds);

		// Attach the MeshPrimitive 
		fr::MeshPrimitiveComponent::AttachMeshPrimitiveComponent(em, owningEntity, spotLightMesh, minPos, maxPos);

		// LightComponent:
		fr::LightComponent& lightComponent = *em.EmplaceComponent<fr::LightComponent>(
			owningEntity,
			PrivateCTORTag{},
			*renderDataComponent,
			fr::Light::Type::Spot,
			colorIntensity,
			hasShadow);
		em.EmplaceComponent<SpotDeferredMarker>(owningEntity);

		// ShadowMapComponent, if required:
		if (hasShadow)
		{
			fr::ShadowMapComponent::AttachShadowMapComponent(
				em, owningEntity, std::format("{}_ShadowMap", name).c_str(), fr::Light::Type::Spot);
		}

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<fr::LightComponent>>(owningEntity);

		return lightComponent;
	}


	LightComponent& LightComponent::AttachDeferredDirectionalLightConcept(
		fr::EntityManager& em,
		entt::entity owningEntity, 
		std::string_view name, 
		glm::vec4 const& colorIntensity,
		bool hasShadow)
	{
		fr::Relationship const& relationship = em.GetComponent<fr::Relationship>(owningEntity);
		fr::TransformComponent const* transformCmpt = relationship.GetFirstInHierarchyAbove<fr::TransformComponent>();
		if (!transformCmpt)
		{
			transformCmpt = &fr::TransformComponent::AttachTransformComponent(em, owningEntity);
		}

		// Note: Our fullscreen quad will technically be linked to the owningTransform; We can't use 
		// k_invalidTransformID as a directional light/shadow needs a valid transform. 
		// Fullscreen quads don't use a Transform so this shouldn't matter.
		fr::RenderDataComponent* renderDataComponent =
			fr::RenderDataComponent::GetCreateRenderDataComponent(em, owningEntity, transformCmpt->GetTransformID());

		// MeshPrimitive:
		core::InvPtr<gr::MeshPrimitive> const& fullscreenQuad =
			gr::meshfactory::CreateFullscreenQuad(em.GetInventory(), gr::meshfactory::ZLocation::Far);

		fr::MeshPrimitiveComponent const& meshPrimCmpt = fr::MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
			em,
			owningEntity,
			*renderDataComponent,
			fullscreenQuad);

		// LightComponent:
		LightComponent& lightComponent = *em.EmplaceComponent<LightComponent>(
			owningEntity,
			PrivateCTORTag{},
			*renderDataComponent,
			fr::Light::Type::Directional,
			colorIntensity,
			hasShadow);
		em.EmplaceComponent<DirectionalDeferredMarker>(owningEntity);

		// ShadowMapComponent, if required:
		if (hasShadow)
		{
			fr::ShadowMapComponent::AttachShadowMapComponent(
				em, owningEntity, std::format("{}_ShadowMap", name).c_str(), fr::Light::Type::Directional);
		}

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<fr::LightComponent>>(owningEntity);

		return lightComponent;
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
		SEAssert(typeProperties.m_ambient.m_IBLTex, "IBL texture cannot be null");

		renderData.m_iblTex = typeProperties.m_ambient.m_IBLTex;

		renderData.m_isActive = typeProperties.m_ambient.m_isActive;

		renderData.m_diffuseScale = typeProperties.m_diffuseEnabled * typeProperties.m_ambient.m_diffuseScale;
		renderData.m_specularScale = typeProperties.m_specularEnabled * typeProperties.m_ambient.m_specularScale;

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

		renderData.m_canContribute = CanLightContribute(
			typeProperties.m_directional.m_colorIntensity, 
			typeProperties.m_diffuseEnabled, 
			typeProperties.m_specularEnabled);

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

		fr::Light::TypeProperties const& typeProperties = light.GetLightTypeProperties(fr::Light::Type::Point);

		renderData.m_colorIntensity = typeProperties.m_point.m_colorIntensity;
		renderData.m_emitterRadius = typeProperties.m_point.m_emitterRadius;
		renderData.m_intensityCuttoff = typeProperties.m_point.m_intensityCuttoff;

		renderData.m_sphericalRadius = typeProperties.m_point.m_sphericalRadius;

		renderData.m_hasShadow = lightCmpt.m_hasShadow;

		renderData.m_canContribute = CanLightContribute(
			typeProperties.m_point.m_colorIntensity,
			typeProperties.m_diffuseEnabled,
			typeProperties.m_specularEnabled);

		renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
		renderData.m_specularEnabled = typeProperties.m_specularEnabled;

		return renderData;
	}


	gr::Light::RenderDataSpot LightComponent::CreateRenderDataSpot_Deferred(
		fr::NameComponent const& nameCmpt, fr::LightComponent const& lightCmpt)
	{
		gr::Light::RenderDataSpot renderData(
			nameCmpt.GetName().c_str(),
			lightCmpt.GetRenderDataID(),
			lightCmpt.GetTransformID());

		fr::Light const& light = lightCmpt.m_light;

		fr::Light::TypeProperties const& typeProperties = light.GetLightTypeProperties(fr::Light::Type::Spot);

		renderData.m_colorIntensity = typeProperties.m_spot.m_colorIntensity;
		renderData.m_emitterRadius = typeProperties.m_spot.m_emitterRadius;
		renderData.m_intensityCuttoff = typeProperties.m_spot.m_intensityCuttoff;

		renderData.m_innerConeAngle = typeProperties.m_spot.m_innerConeAngle;
		renderData.m_outerConeAngle = typeProperties.m_spot.m_outerConeAngle;
		renderData.m_coneHeight = typeProperties.m_spot.m_coneHeight;

		renderData.m_hasShadow = lightCmpt.m_hasShadow;

		renderData.m_canContribute = CanLightContribute(
			typeProperties.m_spot.m_colorIntensity,
			typeProperties.m_diffuseEnabled,
			typeProperties.m_specularEnabled);

		renderData.m_diffuseEnabled = typeProperties.m_diffuseEnabled;
		renderData.m_specularEnabled = typeProperties.m_specularEnabled;

		return renderData;
	}


	void LightComponent::Update(
		entt::entity entity, fr::LightComponent& lightComponent, fr::Transform* lightTransform, fr::Camera* shadowCam)
	{
		fr::Light& light = lightComponent.GetLight();

		bool didModify = light.Update();

		if (light.GetType() != fr::Light::Type::AmbientIBL && lightTransform->HasChanged())
		{
			didModify = true;
		}

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
				SEAssert(lightTransform, "Point lights require a Transform");

				fr::Light::TypeProperties const& lightProperties = light.GetLightTypeProperties(fr::Light::Type::Point);

				// Scale the owning transform such that a sphere created with a radius of 1 will be the correct size
				lightTransform->SetLocalScale(glm::vec3(lightProperties.m_point.m_sphericalRadius));
			}
			break;
			case fr::Light::Type::Spot:
			{
				SEAssert(lightTransform, "Spot lights require a Transform");

				fr::Light::TypeProperties const& lightProperties = light.GetLightTypeProperties(fr::Light::Type::Spot);

				// Scale the owning transform such that a cone created with a height of 1 will be the correct dimensions
				lightTransform->SetLocalScale(
					ComputeConeMeshScale(lightProperties.m_spot.m_outerConeAngle, lightProperties.m_spot.m_coneHeight));

			}
			break;
			default: SEAssertF("Invalid light type");
			}
		}

		if (didModify)
		{
			fr::EntityManager::Get()->TryEmplaceComponent<DirtyMarker<fr::LightComponent>>(entity);
		}
	}


	void LightComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity lightEntity)
	{
		fr::NameComponent const& nameCmpt = em.GetComponent<fr::NameComponent>(lightEntity);

		if (ImGui::CollapsingHeader(
			std::format("Light \"{}\"##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			fr::RenderDataComponent::ShowImGuiWindow(em, lightEntity);

			fr::LightComponent& lightCmpt = em.GetComponent<fr::LightComponent>(lightEntity);
			
			lightCmpt.GetLight().ShowImGuiWindow(nameCmpt.GetUniqueID());

			// Transform:
			fr::TransformComponent* transformComponent = em.TryGetComponent<fr::TransformComponent>(lightEntity);
			SEAssert(transformComponent || lightCmpt.m_light.GetType() == fr::Light::Type::AmbientIBL,
				"Failed to find TransformComponent");
			if (transformComponent)
			{
				fr::TransformComponent::ShowImGuiWindow(em, lightEntity, static_cast<uint64_t>(lightEntity));
			}

			fr::BoundsComponent* boundsCmpt = em.TryGetComponent<fr::BoundsComponent>(lightEntity);
			SEAssert(boundsCmpt || 
				lightCmpt.m_light.GetType() == fr::Light::Type::AmbientIBL || 
				lightCmpt.m_light.GetType() == fr::Light::Type::Directional,
				"Failed to find BoundsComponent");
			if (boundsCmpt)
			{
				fr::BoundsComponent::ShowImGuiWindow(em, lightEntity);
			}

			// Shadow map
			fr::ShadowMapComponent* shadowMapCmpt = em.TryGetComponent<fr::ShadowMapComponent>(lightEntity);
			if (shadowMapCmpt)
			{
				fr::ShadowMapComponent::ShowImGuiWindow(em, lightEntity);
			}

			ImGui::Unindent();
		}
	}


	void LightComponent::ShowImGuiSpawnWindow()
	{
		struct LightSpawnParams
		{
			bool m_attachShadow = true;
			glm::vec4 m_colorIntensity = glm::vec4(1.f, 1.f, 1.f, 100.f);
		};

		auto InitializeSpawnParams = [](fr::Light::Type lightType, std::unique_ptr<LightSpawnParams>& spawnParams)
			{
				spawnParams = std::make_unique<LightSpawnParams>(lightType);
			};

		static fr::Light::Type s_selectedLightType = static_cast<fr::Light::Type>(0);
		static std::unique_ptr<LightSpawnParams> s_spawnParams = std::make_unique<LightSpawnParams>(s_selectedLightType);

		const fr::Light::Type currentSelectedLightTypeIdx = s_selectedLightType;
		util::ShowBasicComboBox(
			"Light type", fr::Light::k_lightTypeNames.data(), fr::Light::k_lightTypeNames.size(), s_selectedLightType);

		// If the selection has changed, re-initialize the spawn parameters:
		if (s_spawnParams == nullptr || s_selectedLightType != currentSelectedLightTypeIdx)
		{
			InitializeSpawnParams(s_selectedLightType, s_spawnParams);
		}

		// Display type-specific spawn options
		switch (s_selectedLightType)
		{
		case fr::Light::Type::AmbientIBL:
		{
			if (ImGui::Button("Import"))
			{
				core::ThreadPool::Get()->EnqueueJob([]()
					{
						std::string filepath;
						if (host::Dialog::OpenFileDialogBox("HDR Files", {"*.hdr"}, filepath))
						{
							core::EventManager::Get()->Notify(core::EventManager::EventInfo{
							.m_eventKey = eventkey::FileImportRequest,
							.m_data = filepath });
						}
					});
			}
		}
		break;
		case fr::Light::Type::Directional:
		case fr::Light::Type::Point:
		case fr::Light::Type::Spot:
		{
			ImGui::Checkbox("Attach shadow map", &s_spawnParams->m_attachShadow);
			ImGui::ColorEdit3("Color",
				&s_spawnParams->m_colorIntensity.r,
				ImGuiColorEditFlags_NoInputs);
			ImGui::SliderFloat("Luminous power", &s_spawnParams->m_colorIntensity.a, 0.f, 1000.f);

			static std::array<char, 64> s_nameInputBuffer = { "Spawned\0" };
			ImGui::InputText("Name", s_nameInputBuffer.data(), s_nameInputBuffer.size());

			if (ImGui::Button("Spawn"))
			{
				fr::EntityManager* em = fr::EntityManager::Get();

				entt::entity sceneNode = fr::SceneNode::Create(*em, s_nameInputBuffer.data(), entt::null);

				switch (static_cast<fr::Light::Type>(s_selectedLightType))
				{
				case fr::Light::Type::AmbientIBL:
				{
					//
				}
				break;
				case fr::Light::Type::Directional:
				{
					fr::LightComponent::AttachDeferredDirectionalLightConcept(
						*em,
						sceneNode,
						std::format("{}_DirectionalLight", s_nameInputBuffer.data()).c_str(),
						s_spawnParams->m_colorIntensity,
						s_spawnParams->m_attachShadow);
				}
				break;
				case fr::Light::Type::Point:
				{
					fr::LightComponent::AttachDeferredPointLightConcept(
						*em,
						sceneNode,
						std::format("{}_PointLight", s_nameInputBuffer.data()).c_str(),
						s_spawnParams->m_colorIntensity,
						s_spawnParams->m_attachShadow);
				}
				break;
				case fr::Light::Type::Spot:
				{
					fr::LightComponent::AttachDeferredSpotLightConcept(
						*em,
						sceneNode,
						std::format("{}_SpotLight", s_nameInputBuffer.data()).c_str(),
						s_spawnParams->m_colorIntensity,
						s_spawnParams->m_attachShadow);
				}
				break;
				default: SEAssertF("Invalid light type");
				}
			}
		}
		break;
		default: SEAssertF("Invalid type");
		}
	}


	// ---


	LightComponent::LightComponent(
		PrivateCTORTag,
		fr::RenderDataComponent const& renderDataComponent, 
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
		fr::RenderDataComponent const& renderDataComponent,
		core::InvPtr<re::Texture> const& iblTex,
		const fr::Light::Type ambientTypeOnly)
		: m_renderDataID(renderDataComponent.GetRenderDataID())
		, m_transformID(renderDataComponent.GetTransformID())
		, m_light(iblTex, fr::Light::Type::AmbientIBL)
		, m_hasShadow(false)
	{
		SEAssert(ambientTypeOnly == fr::Light::Type::AmbientIBL, "This constructor is for ambient light types only");
	}


	// ---


	UpdateLightDataRenderCommand::UpdateLightDataRenderCommand(
		fr::NameComponent const& nameComponent, LightComponent const& lightComponent)
		: m_renderDataID(lightComponent.GetRenderDataID())
		, m_transformID(lightComponent.GetTransformID())
	{
		m_type = fr::Light::ConvertToGrLightType(lightComponent.GetLight().GetType());
		switch (m_type)
		{
		case gr::Light::Type::AmbientIBL:
		{
			// Zero initialize the union, as it contains an InvPtr
			memset(&m_ambientData, 0, sizeof(gr::Light::RenderDataAmbientIBL));

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
		case gr::Light::Type::Spot:
		{
			m_spotData = fr::LightComponent::CreateRenderDataSpot_Deferred(nameComponent, lightComponent);
		}
		break;
		default: SEAssertF("Invalid type");
		}
	}


	UpdateLightDataRenderCommand::~UpdateLightDataRenderCommand()
	{
		switch (m_type)
		{
		case gr::Light::Type::AmbientIBL:
		{
			m_ambientData.m_iblTex = nullptr; // Make sure we don't leak
		}
		break;
		case gr::Light::Type::Directional:
		{
			//
		}
		break;
		case gr::Light::Type::Point:
		{
			//
		}
		break;
		case gr::Light::Type::Spot:
		{
			//
		}
		break;
		default: SEAssertF("Invalid type");
		}
	}


	void UpdateLightDataRenderCommand::Execute(void* cmdData)
	{
		UpdateLightDataRenderCommand* cmdPtr = reinterpret_cast<UpdateLightDataRenderCommand*>(cmdData);

		gr::RenderDataManager& renderDataMgr = re::RenderManager::Get()->GetRenderDataManagerForModification();

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
		case gr::Light::Type::Spot:
		{
			renderDataMgr.SetObjectData<gr::Light::RenderDataSpot>(
				cmdPtr->m_renderDataID, &cmdPtr->m_spotData);
		}
		break;
		default: SEAssertF("Invalid type");
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
		, m_type(fr::Light::ConvertToGrLightType(lightCmpt.GetLight().GetType()))
	{
	}


	void DestroyLightDataRenderCommand::Execute(void* cmdData)
	{
		DestroyLightDataRenderCommand* cmdPtr = reinterpret_cast<DestroyLightDataRenderCommand*>(cmdData);

		gr::RenderDataManager& renderDataMgr = re::RenderManager::Get()->GetRenderDataManagerForModification();

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
		case fr::Light::Type::Spot:
		{
			renderDataMgr.DestroyObjectData<gr::Light::RenderDataSpot>(cmdPtr->m_renderDataID);
		}
		break;
		default: SEAssertF("Invalid type");
		}
	}


	void DestroyLightDataRenderCommand::Destroy(void* cmdData)
	{
		DestroyLightDataRenderCommand* cmdPtr = reinterpret_cast<DestroyLightDataRenderCommand*>(cmdData);
		cmdPtr->~DestroyLightDataRenderCommand();
	}
}