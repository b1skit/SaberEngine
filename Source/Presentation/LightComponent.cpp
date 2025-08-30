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
#include "Renderer/RenderDataManager.h"


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


namespace pr
{
	entt::entity LightComponent::CreateImageBasedLightConcept(
		EntityManager& em, std::string_view name, core::InvPtr<re::Texture> const& iblTex)
	{
		SEAssert(!name.empty() && iblTex, "IBL name or texture cannot be null");

		entt::entity lightEntity = em.CreateEntity(name);

		// MeshPrimitive:
		pr::RenderDataComponent* renderDataComponent = 
			pr::RenderDataComponent::GetCreateRenderDataComponent(em, lightEntity, gr::k_invalidTransformID);

		core::InvPtr<gr::MeshPrimitive> const& fullscreenQuad = 
			gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Far);

		pr::MeshPrimitiveComponent const& meshPrimCmpt = pr::MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
			em,
			lightEntity,
			*renderDataComponent,
			fullscreenQuad);

		// LightComponent:
		pr::LightComponent& lightComponent = *em.EmplaceComponent<pr::LightComponent>(
			lightEntity, 
			PrivateCTORTag{}, 
			*renderDataComponent,
			iblTex);
		em.EmplaceComponent<IBLDeferredMarker>(lightEntity);

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<pr::LightComponent>>(lightEntity);

		return lightEntity;
	}


	LightComponent& LightComponent::AttachDeferredPointLightConcept(
		pr::EntityManager& em,
		entt::entity owningEntity, 
		std::string_view name, 
		glm::vec4 const& colorIntensity, 
		bool hasShadow)
	{
		// Create a MeshPrimitive:
		glm::vec3 minPos = glm::vec3(0.f);
		glm::vec3 maxPos = glm::vec3(0.f);
		const gr::meshfactory::FactoryOptions sphereOptions{ 
			.m_positionMinXYZOut = &minPos, 
			.m_positionMaxXYZOut = &maxPos};

		core::InvPtr<gr::MeshPrimitive> const& pointLightMesh = 
			gr::meshfactory::CreateSphere(sphereOptions, 1.f);

		pr::Relationship const& relationship = em.GetComponent<pr::Relationship>(owningEntity);

		pr::TransformComponent const* transformCmpt = relationship.GetFirstInHierarchyAbove<pr::TransformComponent>(em);
		if (!transformCmpt)
		{
			transformCmpt = &pr::TransformComponent::AttachTransformComponent(em, owningEntity);
		}

		pr::RenderDataComponent* renderDataComponent =
			pr::RenderDataComponent::GetCreateRenderDataComponent(em, owningEntity, transformCmpt->GetTransformID());

		renderDataComponent->SetFeatureBit(gr::RenderObjectFeature::IsLightBounds);

		// Attach the MeshPrimitive 
		pr::MeshPrimitiveComponent::AttachMeshPrimitiveComponent(em, owningEntity, pointLightMesh, minPos, maxPos);

		// LightComponent:
		pr::LightComponent& lightComponent = *em.EmplaceComponent<pr::LightComponent>(
			owningEntity,
			PrivateCTORTag{}, 
			*renderDataComponent,
			pr::Light::Type::Point,
			colorIntensity,
			hasShadow);
		em.EmplaceComponent<PointDeferredMarker>(owningEntity);

		// ShadowMapComponent, if required:
		if (hasShadow)
		{
			pr::ShadowMapComponent::AttachShadowMapComponent(
				em, owningEntity, std::format("{}_ShadowMap", name).c_str(), pr::Light::Type::Point);
		}

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<pr::LightComponent>>(owningEntity);

		return lightComponent;
	}



	LightComponent& LightComponent::AttachDeferredSpotLightConcept(
		pr::EntityManager& em,
		entt::entity owningEntity,
		std::string_view name,
		glm::vec4 const& colorIntensity,
		bool hasShadow)
	{
		// Create a MeshPrimitive:
		glm::vec3 minPos(0.f);
		glm::vec3 maxPos(0.f);

		const gr::meshfactory::FactoryOptions coneFactoryOptions{ 
			.m_orientation = gr::meshfactory::Orientation::ZNegative,
			.m_positionMinXYZOut = &minPos, 
			.m_positionMaxXYZOut = &maxPos
		};

		core::InvPtr<gr::MeshPrimitive> const& spotLightMesh = gr::meshfactory::CreateCone(
			coneFactoryOptions,
			1.f,	// Height
			1.f,	// Radius
			16);	// No. sides

		pr::Relationship const& relationship = em.GetComponent<pr::Relationship>(owningEntity);

		pr::TransformComponent const* transformCmpt = relationship.GetFirstInHierarchyAbove<pr::TransformComponent>(em);
		if (!transformCmpt)
		{
			transformCmpt = &pr::TransformComponent::AttachTransformComponent(em, owningEntity);
		}

		pr::RenderDataComponent* renderDataComponent =
			pr::RenderDataComponent::GetCreateRenderDataComponent(em, owningEntity, transformCmpt->GetTransformID());

		renderDataComponent->SetFeatureBit(gr::RenderObjectFeature::IsLightBounds);

		// Attach the MeshPrimitive 
		pr::MeshPrimitiveComponent::AttachMeshPrimitiveComponent(em, owningEntity, spotLightMesh, minPos, maxPos);

		// LightComponent:
		pr::LightComponent& lightComponent = *em.EmplaceComponent<pr::LightComponent>(
			owningEntity,
			PrivateCTORTag{},
			*renderDataComponent,
			pr::Light::Type::Spot,
			colorIntensity,
			hasShadow);
		em.EmplaceComponent<SpotDeferredMarker>(owningEntity);

		// ShadowMapComponent, if required:
		if (hasShadow)
		{
			pr::ShadowMapComponent::AttachShadowMapComponent(
				em, owningEntity, std::format("{}_ShadowMap", name).c_str(), pr::Light::Type::Spot);
		}

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<pr::LightComponent>>(owningEntity);

		return lightComponent;
	}


	LightComponent& LightComponent::AttachDeferredDirectionalLightConcept(
		pr::EntityManager& em,
		entt::entity owningEntity, 
		std::string_view name, 
		glm::vec4 const& colorIntensity,
		bool hasShadow)
	{
		pr::Relationship const& relationship = em.GetComponent<pr::Relationship>(owningEntity);
		pr::TransformComponent const* transformCmpt = relationship.GetFirstInHierarchyAbove<pr::TransformComponent>(em);
		if (!transformCmpt)
		{
			transformCmpt = &pr::TransformComponent::AttachTransformComponent(em, owningEntity);
		}

		// Note: Our fullscreen quad will technically be linked to the owningTransform; We can't use 
		// k_invalidTransformID as a directional light/shadow needs a valid transform. 
		// Fullscreen quads don't use a Transform so this shouldn't matter.
		pr::RenderDataComponent* renderDataComponent =
			pr::RenderDataComponent::GetCreateRenderDataComponent(em, owningEntity, transformCmpt->GetTransformID());

		// MeshPrimitive:
		core::InvPtr<gr::MeshPrimitive> const& fullscreenQuad =
			gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Far);

		pr::MeshPrimitiveComponent const& meshPrimCmpt = pr::MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
			em,
			owningEntity,
			*renderDataComponent,
			fullscreenQuad);

		// LightComponent:
		LightComponent& lightComponent = *em.EmplaceComponent<LightComponent>(
			owningEntity,
			PrivateCTORTag{},
			*renderDataComponent,
			pr::Light::Type::Directional,
			colorIntensity,
			hasShadow);
		em.EmplaceComponent<DirectionalDeferredMarker>(owningEntity);

		// ShadowMapComponent, if required:
		if (hasShadow)
		{
			pr::ShadowMapComponent::AttachShadowMapComponent(
				em, owningEntity, std::format("{}_ShadowMap", name).c_str(), pr::Light::Type::Directional);
		}

		// Mark our new LightComponent as dirty:
		em.EmplaceComponent<DirtyMarker<pr::LightComponent>>(owningEntity);

		return lightComponent;
	}




	gr::Light::RenderDataIBL LightComponent::CreateRenderDataAmbientIBL_Deferred(
		pr::NameComponent const& nameCmpt, pr::LightComponent const& lightCmpt)
	{
		gr::Light::RenderDataIBL renderData(
			nameCmpt.GetName().c_str(),
			lightCmpt.GetRenderDataID(),
			lightCmpt.GetTransformID());

		pr::Light const& light = lightCmpt.m_light;

		pr::Light::TypeProperties const& typeProperties =
			light.GetLightTypeProperties(pr::Light::Type::IBL);
		SEAssert(typeProperties.m_ibl.m_IBLTex, "IBL texture cannot be null");

		renderData.m_iblTex = typeProperties.m_ibl.m_IBLTex;

		renderData.m_isActive = typeProperties.m_ibl.m_isActive;

		renderData.m_diffuseScale = typeProperties.m_diffuseEnabled * typeProperties.m_ibl.m_diffuseScale;
		renderData.m_specularScale = typeProperties.m_specularEnabled * typeProperties.m_ibl.m_specularScale;

		return renderData;
	}


	gr::Light::RenderDataDirectional LightComponent::CreateRenderDataDirectional_Deferred(
		pr::NameComponent const& nameCmpt, pr::LightComponent const& lightCmpt)
	{
		gr::Light::RenderDataDirectional renderData(
			nameCmpt.GetName().c_str(),
			lightCmpt.GetRenderDataID(),
			lightCmpt.GetTransformID());

		pr::Light const& light = lightCmpt.m_light;

		pr::Light::TypeProperties const& typeProperties =
			light.GetLightTypeProperties(pr::Light::Type::Directional);

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
		pr::NameComponent const& nameCmpt, pr::LightComponent const& lightCmpt)
	{
		gr::Light::RenderDataPoint renderData(
			nameCmpt.GetName().c_str(),
			lightCmpt.GetRenderDataID(),
			lightCmpt.GetTransformID());

		pr::Light const& light = lightCmpt.m_light;

		pr::Light::TypeProperties const& typeProperties = light.GetLightTypeProperties(pr::Light::Type::Point);

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
		pr::NameComponent const& nameCmpt, pr::LightComponent const& lightCmpt)
	{
		gr::Light::RenderDataSpot renderData(
			nameCmpt.GetName().c_str(),
			lightCmpt.GetRenderDataID(),
			lightCmpt.GetTransformID());

		pr::Light const& light = lightCmpt.m_light;

		pr::Light::TypeProperties const& typeProperties = light.GetLightTypeProperties(pr::Light::Type::Spot);

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
		pr::EntityManager& em,
		entt::entity entity,
		pr::LightComponent& lightComponent,
		pr::Transform* lightTransform,
		pr::Camera* shadowCam)
	{
		pr::Light& light = lightComponent.GetLight();

		bool didModify = light.Update();

		if (light.GetType() != pr::Light::Type::IBL && lightTransform->HasChanged())
		{
			didModify = true;
		}

		if (didModify)
		{
			switch (light.GetType())
			{
			case pr::Light::Type::IBL:
			{
				//
			}
			break;
			case pr::Light::Type::Directional:
			{
				//
			}
			break;
			case pr::Light::Type::Point:
			{
				SEAssert(lightTransform, "Point lights require a Transform");

				pr::Light::TypeProperties const& lightProperties = light.GetLightTypeProperties(pr::Light::Type::Point);

				// Scale the owning transform such that a sphere created with a radius of 1 will be the correct size
				lightTransform->SetLocalScale(glm::vec3(lightProperties.m_point.m_sphericalRadius));
			}
			break;
			case pr::Light::Type::Spot:
			{
				SEAssert(lightTransform, "Spot lights require a Transform");

				pr::Light::TypeProperties const& lightProperties = light.GetLightTypeProperties(pr::Light::Type::Spot);

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
			em.TryEmplaceComponent<DirtyMarker<pr::LightComponent>>(entity);
		}
	}


	void LightComponent::ShowImGuiWindow(pr::EntityManager& em, entt::entity lightEntity)
	{
		pr::NameComponent const& nameCmpt = em.GetComponent<pr::NameComponent>(lightEntity);

		if (ImGui::CollapsingHeader(
			std::format("Light \"{}\"##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			pr::RenderDataComponent::ShowImGuiWindow(em, lightEntity);

			pr::LightComponent& lightCmpt = em.GetComponent<pr::LightComponent>(lightEntity);
			
			lightCmpt.GetLight().ShowImGuiWindow(nameCmpt.GetUniqueID());

			// Transform:
			pr::TransformComponent* transformComponent = em.TryGetComponent<pr::TransformComponent>(lightEntity);
			SEAssert(transformComponent || lightCmpt.m_light.GetType() == pr::Light::Type::IBL,
				"Failed to find TransformComponent");
			if (transformComponent)
			{
				pr::TransformComponent::ShowImGuiWindow(em, lightEntity, static_cast<uint64_t>(lightEntity));
			}

			pr::BoundsComponent* boundsCmpt = em.TryGetComponent<pr::BoundsComponent>(lightEntity);
			SEAssert(boundsCmpt || 
				lightCmpt.m_light.GetType() == pr::Light::Type::IBL || 
				lightCmpt.m_light.GetType() == pr::Light::Type::Directional,
				"Failed to find BoundsComponent");
			if (boundsCmpt)
			{
				pr::BoundsComponent::ShowImGuiWindow(em, lightEntity);
			}

			// Shadow map
			pr::ShadowMapComponent* shadowMapCmpt = em.TryGetComponent<pr::ShadowMapComponent>(lightEntity);
			if (shadowMapCmpt)
			{
				pr::ShadowMapComponent::ShowImGuiWindow(em, lightEntity);
			}

			ImGui::Unindent();
		}
	}


	void LightComponent::ShowImGuiSpawnWindow(pr::EntityManager& em)
	{
		struct LightSpawnParams
		{
			bool m_attachShadow = true;
			glm::vec4 m_colorIntensity = glm::vec4(1.f, 1.f, 1.f, 100.f);
		};

		auto InitializeSpawnParams = [](pr::Light::Type lightType, std::unique_ptr<LightSpawnParams>& spawnParams)
			{
				spawnParams = std::make_unique<LightSpawnParams>(lightType);
			};

		static pr::Light::Type s_selectedLightType = static_cast<pr::Light::Type>(0);
		static std::unique_ptr<LightSpawnParams> s_spawnParams = std::make_unique<LightSpawnParams>(s_selectedLightType);

		const pr::Light::Type currentSelectedLightTypeIdx = s_selectedLightType;
		util::ShowBasicComboBox(
			"Light type", pr::Light::k_lightTypeNames.data(), pr::Light::k_lightTypeNames.size(), s_selectedLightType);

		// If the selection has changed, re-initialize the spawn parameters:
		if (s_spawnParams == nullptr || s_selectedLightType != currentSelectedLightTypeIdx)
		{
			InitializeSpawnParams(s_selectedLightType, s_spawnParams);
		}

		// Display type-specific spawn options
		switch (s_selectedLightType)
		{
		case pr::Light::Type::IBL:
		{
			if (ImGui::Button("Import"))
			{
				core::ThreadPool::EnqueueJob([]()
					{
						std::string filepath;
						if (host::Dialog::OpenFileDialogBox("HDR Files", {"*.hdr"}, filepath))
						{
							core::EventManager::Notify(core::EventManager::EventInfo{
							.m_eventKey = eventkey::FileImportRequest,
							.m_data = filepath });
						}
					});
			}
		}
		break;
		case pr::Light::Type::Directional:
		case pr::Light::Type::Point:
		case pr::Light::Type::Spot:
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
				entt::entity sceneNode = pr::SceneNode::Create(em, s_nameInputBuffer.data(), entt::null);

				switch (static_cast<pr::Light::Type>(s_selectedLightType))
				{
				case pr::Light::Type::IBL:
				{
					//
				}
				break;
				case pr::Light::Type::Directional:
				{
					pr::LightComponent::AttachDeferredDirectionalLightConcept(
						em,
						sceneNode,
						std::format("{}_DirectionalLight", s_nameInputBuffer.data()).c_str(),
						s_spawnParams->m_colorIntensity,
						s_spawnParams->m_attachShadow);
				}
				break;
				case pr::Light::Type::Point:
				{
					pr::LightComponent::AttachDeferredPointLightConcept(
						em,
						sceneNode,
						std::format("{}_PointLight", s_nameInputBuffer.data()).c_str(),
						s_spawnParams->m_colorIntensity,
						s_spawnParams->m_attachShadow);
				}
				break;
				case pr::Light::Type::Spot:
				{
					pr::LightComponent::AttachDeferredSpotLightConcept(
						em,
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
		pr::RenderDataComponent const& renderDataComponent, 
		pr::Light::Type lightType, 
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
		pr::RenderDataComponent const& renderDataComponent,
		core::InvPtr<re::Texture> const& iblTex,
		const pr::Light::Type ambientTypeOnly)
		: m_renderDataID(renderDataComponent.GetRenderDataID())
		, m_transformID(renderDataComponent.GetTransformID())
		, m_light(iblTex, pr::Light::Type::IBL)
		, m_hasShadow(false)
	{
		SEAssert(ambientTypeOnly == pr::Light::Type::IBL, "This constructor is for ambient light types only");
	}


	// ---


	UpdateLightDataRenderCommand::UpdateLightDataRenderCommand(
		pr::NameComponent const& nameComponent, LightComponent const& lightComponent)
		: m_renderDataID(lightComponent.GetRenderDataID())
		, m_transformID(lightComponent.GetTransformID())
	{
		m_type = pr::Light::ConvertToGrLightType(lightComponent.GetLight().GetType());
		switch (m_type)
		{
		case gr::Light::Type::IBL:
		{
			// Zero initialize the union, as it contains an InvPtr
			memset(&m_ambientData, 0, sizeof(gr::Light::RenderDataIBL));

			m_ambientData = pr::LightComponent::CreateRenderDataAmbientIBL_Deferred(nameComponent, lightComponent);
		}
		break;
		case gr::Light::Type::Directional:
		{
			m_directionalData = pr::LightComponent::CreateRenderDataDirectional_Deferred(nameComponent, lightComponent);
		}
		break;
		case gr::Light::Type::Point:
		{
			m_pointData = pr::LightComponent::CreateRenderDataPoint_Deferred(nameComponent, lightComponent);
		}
		break;
		case gr::Light::Type::Spot:
		{
			m_spotData = pr::LightComponent::CreateRenderDataSpot_Deferred(nameComponent, lightComponent);
		}
		break;
		default: SEAssertF("Invalid type");
		}
	}


	UpdateLightDataRenderCommand::~UpdateLightDataRenderCommand()
	{
		switch (m_type)
		{
		case gr::Light::Type::IBL:
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

		gr::RenderDataManager& renderDataMgr = cmdPtr->GetRenderDataManagerForModification();

		switch (cmdPtr->m_type)
		{
		case gr::Light::Type::IBL:
		{
			renderDataMgr.SetObjectData<gr::Light::RenderDataIBL>(
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


	// ---


	DestroyLightDataRenderCommand::DestroyLightDataRenderCommand(LightComponent const& lightCmpt)
		: m_renderDataID(lightCmpt.GetRenderDataID())
		, m_type(pr::Light::ConvertToGrLightType(lightCmpt.GetLight().GetType()))
	{
	}


	void DestroyLightDataRenderCommand::Execute(void* cmdData)
	{
		DestroyLightDataRenderCommand* cmdPtr = reinterpret_cast<DestroyLightDataRenderCommand*>(cmdData);

		gr::RenderDataManager& renderDataMgr = cmdPtr->GetRenderDataManagerForModification();

		switch (cmdPtr->m_type)
		{
		case pr::Light::Type::IBL:
		{
			renderDataMgr.DestroyObjectData<gr::Light::RenderDataIBL>(cmdPtr->m_renderDataID);
		}
		break;
		case pr::Light::Type::Directional:
		{
			renderDataMgr.DestroyObjectData<gr::Light::RenderDataDirectional>(cmdPtr->m_renderDataID);
		}
		break;
		case pr::Light::Type::Point:
		{
			renderDataMgr.DestroyObjectData<gr::Light::RenderDataPoint>(cmdPtr->m_renderDataID);
		}
		break;
		case pr::Light::Type::Spot:
		{
			renderDataMgr.DestroyObjectData<gr::Light::RenderDataSpot>(cmdPtr->m_renderDataID);
		}
		break;
		default: SEAssertF("Invalid type");
		}
	}
}