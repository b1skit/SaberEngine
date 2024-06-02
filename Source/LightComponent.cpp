// © 2023 Adam Badke. All rights reserved.
#include "Camera.h"
#include "EntityCommands.h"
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
#include "SceneNodeConcept.h"
#include "ShadowMapComponent.h"
#include "TransformComponent.h"

#include "Core/Config.h"
#include "Core/Util/FileIOUtils.h"
#include "Core/Util/ImGuiUtils.h"


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
	entt::entity LightComponent::CreateDeferredAmbientLightConcept(EntityManager& em, re::Texture const* iblTex)
	{
		SEAssert(iblTex, "IBL texture cannot be null");

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

		return lightEntity;
	}


	LightComponent& LightComponent::AttachDeferredPointLightConcept(
		fr::EntityManager& em,
		entt::entity owningEntity, 
		char const* name, 
		glm::vec4 const& colorIntensity, 
		bool hasShadow)
	{
		SEAssert(em.HasComponent<fr::TransformComponent>(owningEntity),
			"A LightComponent's owning entity requires a TransformComponent");

		// Create a MeshPrimitive (owned by SceneData):
		std::shared_ptr<gr::MeshPrimitive> pointLightMesh = 
			gr::meshfactory::CreateSphere(gr::meshfactory::FactoryOptions{}, 1.f);

		fr::TransformComponent& owningTransform = em.GetComponent<fr::TransformComponent>(owningEntity);

		gr::RenderDataComponent& renderDataComponent =
			gr::RenderDataComponent::AttachNewRenderDataComponent(em, owningEntity, owningTransform.GetTransformID());

		// Attach the MeshPrimitive 
		fr::MeshPrimitiveComponent::AttachMeshPrimitiveComponent(em, owningEntity, pointLightMesh.get());

		// LightComponent:
		fr::LightComponent& lightComponent = *em.EmplaceComponent<fr::LightComponent>(
			owningEntity,
			PrivateCTORTag{}, 
			renderDataComponent,
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


	LightComponent& LightComponent::AttachDeferredPointLightConcept(
		fr::EntityManager& em, 
		entt::entity entity, 
		std::string const& name, 
		glm::vec4 const& colorIntensity, 
		bool hasShadow)
	{
		return AttachDeferredPointLightConcept(em, entity, name.c_str(), colorIntensity, hasShadow);
	}


	LightComponent& LightComponent::AttachDeferredSpotLightConcept(
		fr::EntityManager& em,
		entt::entity owningEntity,
		char const* name,
		glm::vec4 const& colorIntensity,
		bool hasShadow)
	{
		SEAssert(em.HasComponent<fr::TransformComponent>(owningEntity),
			"A LightComponent's owning entity requires a TransformComponent");

		// Create a MeshPrimitive (owned by SceneData):
		gr::meshfactory::FactoryOptions coneFactoryOptions{};
		coneFactoryOptions.m_orientation = gr::meshfactory::Orientation::ZNegative;

		std::shared_ptr<gr::MeshPrimitive> spotLightMesh = gr::meshfactory::CreateCone(
			coneFactoryOptions,
			1.f,	// Height
			1.f,	// Radius
			16);	// No. sides		

		fr::TransformComponent& owningTransform = em.GetComponent<fr::TransformComponent>(owningEntity);

		gr::RenderDataComponent& renderDataComponent =
			gr::RenderDataComponent::AttachNewRenderDataComponent(em, owningEntity, owningTransform.GetTransformID());

		// Attach the MeshPrimitive 
		fr::MeshPrimitiveComponent::AttachMeshPrimitiveComponent(em, owningEntity, spotLightMesh.get());

		// LightComponent:
		fr::LightComponent& lightComponent = *em.EmplaceComponent<fr::LightComponent>(
			owningEntity,
			PrivateCTORTag{},
			renderDataComponent,
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


	LightComponent& LightComponent::AttachDeferredSpotLightConcept(
		fr::EntityManager& em,
		entt::entity entity,
		std::string const& name,
		glm::vec4 const& colorIntensity,
		bool hasShadow)
	{
		return AttachDeferredSpotLightConcept(em, entity, name.c_str(), colorIntensity, hasShadow);
	}


	LightComponent& LightComponent::AttachDeferredDirectionalLightConcept(
		fr::EntityManager& em,
		entt::entity owningEntity, 
		char const* name, 
		glm::vec4 const& colorIntensity,
		bool hasShadow)
	{
		SEAssert(em.HasComponent<fr::TransformComponent>(owningEntity),
			"A light's owning entity requires a TransformComponent");

		fr::TransformComponent& owningTransform = em.GetComponent<fr::TransformComponent>(owningEntity);

		// Note: Our fullscreen quad will technically be linked to the owningTransform; We can't use 
		// k_sharedIdentityTransformID as a directional light/shadow needs a valid transform. 
		// Fullscreen quads don't use a Transform so this shouldn't matter.
		gr::RenderDataComponent& renderDataComponent =
			gr::RenderDataComponent::AttachNewRenderDataComponent(em, owningEntity, owningTransform.GetTransformID());

		// MeshPrimitive:
		std::shared_ptr<gr::MeshPrimitive> fullscreenQuadSceneData =
			gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Far);

		fr::MeshPrimitiveComponent const& meshPrimCmpt = fr::MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
			em,
			owningEntity,
			renderDataComponent,
			fullscreenQuadSceneData.get());

		// LightComponent:
		LightComponent& lightComponent = *em.EmplaceComponent<LightComponent>(
			owningEntity,
			PrivateCTORTag{},
			renderDataComponent,
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


	LightComponent& LightComponent::AttachDeferredDirectionalLightConcept(
		fr::EntityManager& em,
		entt::entity entity, 
		std::string const& name, 
		glm::vec4 const& colorIntensity,
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


	bool LightComponent::Update(fr::LightComponent& lightComponent, fr::Transform* lightTransform, fr::Camera* shadowCam)
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

		return didModify;
	}


	void LightComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity lightEntity)
	{
		fr::NameComponent const& nameCmpt = em.GetComponent<fr::NameComponent>(lightEntity);

		if (ImGui::CollapsingHeader(
			std::format("Light \"{}\"##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, lightEntity);

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
		union LightSpawnParams
		{
			LightSpawnParams() { memset(this, 0, sizeof(LightSpawnParams)); }
			~LightSpawnParams() {}

			struct AmbientLightSpawnParams
			{
				std::string m_filepath;
			} m_ambientLightSpawnParams;

			struct PunctualLightSpawnParams
			{
				bool m_attachShadow;
				glm::vec4 m_colorIntensity;
			} m_punctualLightSpawnParams;
		};

		static std::unique_ptr<LightSpawnParams> s_spawnParams = std::make_unique<LightSpawnParams>();

		auto InitializeSpawnParams = [](std::unique_ptr<LightSpawnParams>& spawnParams)
			{
				spawnParams = std::make_unique<LightSpawnParams>();
			};

		static fr::Light::Type s_selectedLightType = static_cast<fr::Light::Type>(0);
		const fr::Light::Type currentSelectedLightTypeIdx = s_selectedLightType;
		util::ShowBasicComboBox(
			"Light type", fr::Light::k_lightTypeNames.data(), fr::Light::k_lightTypeNames.size(), s_selectedLightType);

		// If the selection has changed, re-initialize the spawn parameters:
		if (s_spawnParams == nullptr || s_selectedLightType != currentSelectedLightTypeIdx)
		{
			InitializeSpawnParams(s_spawnParams);
		}

		// Display type-specific spawn options
		switch (static_cast<fr::Light::Type>(s_selectedLightType))
		{
		case fr::Light::Type::AmbientIBL:
		{
			std::vector<std::string> const& iblHDRFiles = util::GetDirectoryFilenameContents(
				core::Config::Get()->GetValue<std::string>(core::configkeys::k_sceneIBLDirKey).c_str(), ".hdr");

			static size_t selectedFileIdx = 0;
			if (ImGui::BeginListBox("Selected source IBL HDR File"))
			{
				for (size_t i = 0; i < iblHDRFiles.size(); i++)
				{
					const bool isSelected = selectedFileIdx == i;
					if (ImGui::Selectable(iblHDRFiles[i].c_str(), isSelected))
					{
						selectedFileIdx = i;
					}
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
						s_spawnParams->m_ambientLightSpawnParams.m_filepath = iblHDRFiles[selectedFileIdx];
					}
				}
				ImGui::EndListBox();
			}
		}
		break;
		case fr::Light::Type::Directional:
		case fr::Light::Type::Point:
		case fr::Light::Type::Spot:
		{
			ImGui::Checkbox("Attach shadow map", &s_spawnParams->m_punctualLightSpawnParams.m_attachShadow);
			ImGui::ColorEdit3("Color",
				&s_spawnParams->m_punctualLightSpawnParams.m_colorIntensity.r,
				ImGuiColorEditFlags_NoInputs);
			ImGui::SliderFloat("Luminous power", &s_spawnParams->m_punctualLightSpawnParams.m_colorIntensity.a, 0.f, 10.f);
		}
		break;
		default: SEAssertF("Invalid type");
		}

		static std::array<char, 64> s_nameInputBuffer = { "Spawned\0" };
		ImGui::InputText("Name", s_nameInputBuffer.data(), s_nameInputBuffer.size());

		if (ImGui::Button("Spawn"))
		{
			entt::entity sceneNode =
				fr::SceneNode::Create(*fr::EntityManager::Get(), s_nameInputBuffer.data(), entt::null);

			switch (static_cast<fr::Light::Type>(s_selectedLightType))
			{
			case fr::Light::Type::AmbientIBL:
			{
				// TODO: THIS SHOULD BE PART OF THE AMBIENT LIGHT LOGIC, NOT IMGUI!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				re::Texture const* newIBL = fr::SceneManager::GetSceneData()->TryLoadUniqueTexture(
					s_spawnParams->m_ambientLightSpawnParams.m_filepath,
					re::Texture::ColorSpace::Linear).get();

				if (newIBL)
				{
					entt::entity newAmbientLight =
						fr::LightComponent::CreateDeferredAmbientLightConcept(*fr::EntityManager::Get(), newIBL);

					fr::EntityManager::Get()->EnqueueEntityCommand<fr::SetActiveAmbientLightCommand>(newAmbientLight);
				}
			}
			break;
			case fr::Light::Type::Directional:
			{
				fr::LightComponent::AttachDeferredDirectionalLightConcept(
					*fr::EntityManager::Get(),
					sceneNode,
					std::format("{}_DirectionalLight", s_nameInputBuffer.data()).c_str(),
					s_spawnParams->m_punctualLightSpawnParams.m_colorIntensity,
					s_spawnParams->m_punctualLightSpawnParams.m_attachShadow);
			}
			break;
			case fr::Light::Type::Point:
			{
				fr::LightComponent::AttachDeferredPointLightConcept(
					*fr::EntityManager::Get(),
					sceneNode,
					std::format("{}_PointLight", s_nameInputBuffer.data()).c_str(),
					s_spawnParams->m_punctualLightSpawnParams.m_colorIntensity,
					s_spawnParams->m_punctualLightSpawnParams.m_attachShadow);
			}
			break;
			case fr::Light::Type::Spot:
			{
				fr::LightComponent::AttachDeferredSpotLightConcept(
					*fr::EntityManager::Get(),
					sceneNode,
					std::format("{}_SpotLight", s_nameInputBuffer.data()).c_str(),
					s_spawnParams->m_punctualLightSpawnParams.m_colorIntensity,
					s_spawnParams->m_punctualLightSpawnParams.m_attachShadow);
			}
			break;
			default: SEAssertF("Invalid light type");
			}
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