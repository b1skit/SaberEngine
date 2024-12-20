// © 2022 Adam Badke. All rights reserved.
#include "AnimationComponent.h"
#include "BoundsComponent.h"
#include "CameraComponent.h"
#include "CameraControlComponent.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "MaterialInstanceComponent.h"
#include "MeshConcept.h"
#include "MeshMorphComponent.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"
#include "ShadowMapComponent.h"
#include "SkinningComponent.h"

#include "Core/Config.h"

#include "Core/Definitions/ConfigKeys.h"

#include "Renderer/RenderManager.h"


namespace
{
	constexpr size_t k_entityCommandBufferSize = 1024;
}

namespace fr
{
	EntityManager* EntityManager::Get()
	{
		static std::unique_ptr<fr::EntityManager> instance = std::make_unique<fr::EntityManager>(PrivateCTORTag{});
		return instance.get();
	}


	EntityManager::EntityManager(PrivateCTORTag)
		: m_processInput(false)
		, m_entityCommands(k_entityCommandBufferSize)
	{
		// Handle this during construction before anything can interact with the registry
		ConfigureRegistry();
	}


	void EntityManager::Startup()
	{
		LOG("EntityManager starting...");

		SEAssert(m_inventory, "Inventory is null. This dependency must be injected immediately after creation");

		// Event subscriptions:
		core::EventManager::Get()->Subscribe(core::EventManager::EventType::InputToggleConsole, this);

		// Process entity commands issued during scene loading:
		ProcessEntityCommands();

		// Create a scene bounds entity:
		fr::BoundsComponent::CreateSceneBoundsConcept(*this);

		// Create an Ambient light, and make it active:
		entt::entity ambientLight = fr::LightComponent::CreateDeferredAmbientLightConcept(
			*this,
			en::DefaultResourceNames::k_defaultIBLTexName,
			m_inventory->Get<re::Texture>(util::StringHash(en::DefaultResourceNames::k_defaultIBLTexName)));
		SetActiveAmbientLight(ambientLight);

		// Add a camera controller to the scene:
		entt::entity mainCameraEntity = GetMainCamera();

		// Only bind non-animated main cameras to the camera controller
		if (!HasComponent<fr::AnimationComponent>(mainCameraEntity))
		{
			fr::CameraControlComponent::CreateCameraControlConcept(*this, mainCameraEntity);

			fr::NameComponent const& mainCameraName = GetComponent<fr::NameComponent>(mainCameraEntity);
			LOG("Attached CameraControlComponent to camera \"%s\"", mainCameraName.GetName().c_str());
		}
		else
		{
			fr::CameraControlComponent::CreateCameraControlConcept(*this, entt::null);
			LOG("Created unbound CameraControlComponent");
		}		

		m_processInput = true;
	}


	void EntityManager::Shutdown()
	{
		LOG("EntityManager shutting down...");

		// Process any remaining entity commands
		ProcessEntityCommands();

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Add all entities to the deferred delete queue
			for (auto entityTuple : m_registry.storage<entt::entity>().each())
			{
				entt::entity entity = std::get<entt::entity>(entityTuple);

				RegisterEntityForDelete(entity);
			}
		}

		ExecuteDeferredDeletions();

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
			m_registry.clear();
		}
	}


	void EntityManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		HandleEvents();

		ProcessEntityCommands();

		// Handle interaction (player input, physics, animation, etc)
		if (m_processInput)
		{
			UpdateCameraController(stepTimeMs);
		}
		
		// Update the scene state:
		UpdateAnimationControllers(stepTimeMs);  // Modifies Transforms
		UpdateTransforms();
		UpdateAnimations(stepTimeMs);
		UpdateBounds();
		UpdateMaterials();
		UpdateLightsAndShadows();
		UpdateCameras();

		ExecuteDeferredDeletions();
	}


	void EntityManager::ProcessEntityCommands()
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
			m_entityCommands.SwapBuffers();
			m_entityCommands.Execute();
		}
	}


	template<typename RenderDataType, typename CmptType, typename... OtherCmpts>
	void EntityManager::EnqueueRenderUpdateHelper()
	{
		re::RenderManager* renderManager = re::RenderManager::Get();

		auto componentsView = m_registry.view<fr::RenderDataComponent, DirtyMarker<CmptType>, CmptType, OtherCmpts...>();
		for (auto entity : componentsView)
		{
			fr::RenderDataComponent const& renderDataComponent = componentsView.get<fr::RenderDataComponent>(entity);

			CmptType const& component = componentsView.get<CmptType>(entity);

			renderManager->EnqueueRenderCommand<fr::UpdateRenderDataRenderCommand<RenderDataType>>(
				renderDataComponent.GetRenderDataID(),
				CmptType::CreateRenderData(entity, component));

			m_registry.erase<DirtyMarker<CmptType>>(entity);
		}
	}


	void EntityManager::EnqueueRenderUpdates()
	{
		re::RenderManager* renderManager = re::RenderManager::Get();

		// ECS_CONVERSION TODO: Move each of these isolated tasks to a thread
		// -> Use entt::organizer

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Register new render objects:
			auto newRenderableEntitiesView = 
				m_registry.view<fr::RenderDataComponent, fr::RenderDataComponent::NewRegistrationMarker>();
			for (auto entity : newRenderableEntitiesView)
			{
				// Enqueue a command to create a new object on the render thread:
				auto& renderDataComponent = newRenderableEntitiesView.get<fr::RenderDataComponent>(entity);

				renderManager->EnqueueRenderCommand<fr::RegisterRenderObjectCommand>(renderDataComponent);
				
				m_registry.erase<fr::RenderDataComponent::NewRegistrationMarker>(entity);
			}

			// Initialize new Transforms associated with a RenderDataComponent:
			auto newTransformComponentsView = 
				m_registry.view<fr::TransformComponent, fr::TransformComponent::NewIDMarker, fr::RenderDataComponent>();
			for (auto entity : newTransformComponentsView)
			{
				fr::TransformComponent& transformComponent =
					newTransformComponentsView.get<fr::TransformComponent>(entity);

				renderManager->EnqueueRenderCommand<fr::UpdateTransformDataRenderCommand>(transformComponent);

				m_registry.erase<fr::TransformComponent::NewIDMarker>(entity);
			}

			// Clear the NewIDMarker from any remaining TransformComponents not associated with a fr::RenderDataComponent
			auto remainingNewTransformsView = 
				m_registry.view<fr::TransformComponent, fr::TransformComponent::NewIDMarker>();
			for (auto entity : remainingNewTransformsView)
			{
				fr::TransformComponent& transformComponent =
					remainingNewTransformsView.get<fr::TransformComponent>(entity);

				m_registry.erase<fr::TransformComponent::NewIDMarker>(entity);
			}

			// Update dirty render data components:
			// ------------------------------------

			// Transforms:
			auto transformCmptsView = m_registry.view<fr::TransformComponent>();
			for (auto entity : transformCmptsView)
			{
				fr::TransformComponent& transformComponent = transformCmptsView.get<fr::TransformComponent>(entity);

				if (transformComponent.GetTransform().HasChanged())
				{
					renderManager->EnqueueRenderCommand<fr::UpdateTransformDataRenderCommand>(transformComponent);
					transformComponent.GetTransform().ClearHasChangedFlag();
				}
			}

			// Handle camera changes:
			auto newMainCameraView = m_registry.view<
				fr::CameraComponent,
				fr::CameraComponent::MainCameraMarker,
				fr::CameraComponent::NewMainCameraMarker,
				fr::RenderDataComponent>();
			for (auto entity : newMainCameraView)
			{
				fr::RenderDataComponent const& renderDataComponent =
					newMainCameraView.get<fr::RenderDataComponent>(entity);

				renderManager->EnqueueRenderCommand<fr::SetActiveCameraRenderCommand>(
					renderDataComponent.GetRenderDataID(), renderDataComponent.GetTransformID());

				m_registry.erase<fr::CameraComponent::NewMainCameraMarker>(entity);
			}

			EnqueueRenderUpdateHelper<gr::Bounds::RenderData, fr::BoundsComponent>();
			EnqueueRenderUpdateHelper<gr::MeshPrimitive::RenderData, fr::MeshPrimitiveComponent>();
			EnqueueRenderUpdateHelper<gr::Material::MaterialInstanceRenderData, fr::MaterialInstanceComponent>();
			EnqueueRenderUpdateHelper<gr::Camera::RenderData, fr::CameraComponent>();
			EnqueueRenderUpdateHelper<gr::MeshPrimitive::MeshMorphRenderData, fr::MeshMorphComponent, 
				fr::Mesh::MeshConceptMarker, fr::AnimationComponent>();
			EnqueueRenderUpdateHelper<gr::MeshPrimitive::SkinningRenderData, fr::SkinningComponent>();

			// Lights:
			auto lightComponentsView = m_registry.view<
				fr::RenderDataComponent, fr::NameComponent, DirtyMarker<fr::LightComponent>, fr::LightComponent>();
			for (auto entity : lightComponentsView)
			{
				fr::NameComponent const& nameComponent = lightComponentsView.get<fr::NameComponent>(entity);
				fr::LightComponent const& lightComponent = lightComponentsView.get<fr::LightComponent>(entity);
				renderManager->EnqueueRenderCommand<fr::UpdateLightDataRenderCommand>(nameComponent, lightComponent);

				m_registry.erase<DirtyMarker<fr::LightComponent>>(entity);
			}

			// Shadows:
			EnqueueRenderUpdateHelper<gr::ShadowMap::RenderData, fr::ShadowMapComponent>();
		}
	}


	fr::BoundsComponent const* EntityManager::GetSceneBounds() const
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto sceneBoundsEntityView = m_registry.view<fr::BoundsComponent, fr::BoundsComponent::SceneBoundsMarker>();
			SEAssert(sceneBoundsEntityView.front() == sceneBoundsEntityView.back(),
				"A unique scene bounds entity must exist");

			const entt::entity sceneBoundsEntity = sceneBoundsEntityView.front();
			if (sceneBoundsEntity != entt::null)
			{
				return &sceneBoundsEntityView.get<fr::BoundsComponent>(sceneBoundsEntity);
			}
			else
			{
				return nullptr;
			}			
		}
	}


	void EntityManager::SetMainCamera(entt::entity newMainCamera)
	{
		SEAssert(newMainCamera != entt::null && HasComponent<fr::CameraComponent>(newMainCamera),
			"Entity does not have a valid camera component");

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			entt::entity currentMainCamera = entt::null;
			bool foundCurrentMainCamera = false;
			auto currentMainCameraView = m_registry.view<fr::CameraComponent::MainCameraMarker>();
			for (auto entity : currentMainCameraView)
			{
				SEAssert(foundCurrentMainCamera == false, "Already found a main camera. This should not be possible");
				foundCurrentMainCamera = true;

				currentMainCamera = entity;

				m_registry.erase<fr::CameraComponent::MainCameraMarker>(entity);

				// If the main camera was added during the current frame, ensure we don't end up with 2 new camera markers
				if (m_registry.any_of<fr::CameraComponent::NewMainCameraMarker>(entity))
				{
					m_registry.erase<fr::CameraComponent::NewMainCameraMarker>(entity);
				}
			}

			m_registry.emplace_or_replace<fr::CameraComponent::MainCameraMarker>(newMainCamera);
			m_registry.emplace_or_replace<fr::CameraComponent::NewMainCameraMarker>(newMainCamera);

			// Find and update the camera controller:
			entt::entity camController = entt::null;
			auto camControllerView = m_registry.view<fr::CameraControlComponent>();
			for (entt::entity entity : camControllerView)
			{
				SEAssert(camController == entt::null, "Already found camera controller. This shouldn't be possible");
				camController = entity;
			}

			if (camController != entt::null) // No point trying to set a camera if the camera controller doesn't exist yet
			{
				// Animated cameras cannot be controlled by a camera controller
				entt::entity camControllerTarget = entt::null;
				if (!m_registry.any_of<fr::AnimationComponent>(newMainCamera))
				{
					camControllerTarget = newMainCamera;
				}
				fr::CameraControlComponent::SetCamera(camController, currentMainCamera, camControllerTarget);
			}
		}
	}


	entt::entity EntityManager::GetMainCamera() const
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
			
			entt::entity mainCamEntity = entt::null;

			bool foundCurrentMainCamera = false;
			auto currentMainCameraView = m_registry.view<fr::CameraComponent::MainCameraMarker>();
			for (auto entity : currentMainCameraView)
			{
				SEAssert(foundCurrentMainCamera == false, "Already found a main camera. This should not be possible");
				foundCurrentMainCamera = true;

				mainCamEntity = entity;
			}
			SEAssert(mainCamEntity != entt::null, "Failed to find a main camera entity");

			return mainCamEntity;
		}
	}


	void EntityManager::SetActiveAmbientLight(entt::entity ambientLight)
	{
		if (ambientLight == entt::null)
		{
			return; // Do nothing
		}


		const entt::entity prevActiveAmbient = GetActiveAmbientLight();

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// We might not have a previously active ambient light, if this is the first ambient light we've added
			if (prevActiveAmbient != entt::null)
			{
				fr::LightComponent& prevActiveLightComponent = GetComponent<fr::LightComponent>(prevActiveAmbient);

				SEAssert(prevActiveLightComponent.GetLight().GetType() == fr::Light::Type::AmbientIBL,
					"Light component is not the correct type");

				fr::Light::TypeProperties prevLightTypeProperties =
					prevActiveLightComponent.GetLight().GetLightTypeProperties(fr::Light::Type::AmbientIBL);

				SEAssert(prevLightTypeProperties.m_ambient.m_isActive,
					"Ambient light is not active. This should not be possible");

				prevLightTypeProperties.m_ambient.m_isActive = false;

				// This will mark the light as dirty, and trigger an update
				prevActiveLightComponent.GetLight().SetLightTypeProperties(
					fr::Light::Type::AmbientIBL, &prevLightTypeProperties.m_ambient);

				RemoveComponent<fr::LightComponent::IsActiveAmbientDeferredMarker>(prevActiveAmbient);
			}

			// Promote the new light to the active one:
			fr::LightComponent& lightComponent = GetComponent<fr::LightComponent>(ambientLight);

			SEAssert(lightComponent.GetLight().GetType() == fr::Light::Type::AmbientIBL,
				"Light component is not the correct type");

			// Update the active flag:
			fr::Light::TypeProperties currentLightTypeProperties =
				lightComponent.GetLight().GetLightTypeProperties(fr::Light::Type::AmbientIBL);

			SEAssert(!currentLightTypeProperties.m_ambient.m_isActive,
				"Ambient light is already active. This is harmless, but unexpected");

			currentLightTypeProperties.m_ambient.m_isActive = true;

			// This will mark the light as dirty, and trigger an update
			lightComponent.GetLight().SetLightTypeProperties(
				fr::Light::Type::AmbientIBL, &currentLightTypeProperties.m_ambient);

			// Mark the new light as the active light:
			EmplaceComponent<fr::LightComponent::IsActiveAmbientDeferredMarker>(ambientLight);
		}
	}


	entt::entity EntityManager::GetActiveAmbientLight() const
	{
		entt::entity activeAmbient = entt::null;
		bool foundCurrentActiveAmbient = false;

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto currentActiveAmbient = m_registry.view<fr::LightComponent::IsActiveAmbientDeferredMarker>();
			for (auto entity : currentActiveAmbient)
			{
				SEAssert(foundCurrentActiveAmbient == false,
					"Already found an active ambient light. This should not be possible");
				foundCurrentActiveAmbient = true;

				activeAmbient = entity;
			}
		}
		// Note: It's possible we won't have an active ambient light (e.g. one hasn't been added yet)

		return activeAmbient;
	}


	entt::entity EntityManager::CreateEntity(std::string const& name)
	{
		return CreateEntity(name.c_str());
	}


	entt::entity EntityManager::CreateEntity(char const* name)
	{
		entt::entity newEntity = entt::null;
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
			newEntity = m_registry.create();
		}

		fr::NameComponent::AttachNameComponent(*this, newEntity, name);
		fr::Relationship::AttachRelationshipComponent(*this, newEntity);

		return newEntity;
	}


	void EntityManager::RegisterEntityForDelete(entt::entity entity)
	{
		{
			std::lock_guard<std::mutex> lock(m_deferredDeleteQueueMutex);
			m_deferredDeleteQueue.emplace_back(entity);
		}
	}


	void EntityManager::ExecuteDeferredDeletions()
	{
		re::RenderManager* renderManager = re::RenderManager::Get();

		if (!m_deferredDeleteQueue.empty())
		{
			std::scoped_lock lock(m_registeryMutex, m_deferredDeleteQueueMutex);

			for (entt::entity entity : m_deferredDeleteQueue)
			{
				// If the entity has a RenderDataComponent, we must enqueue delete commands for the render thread
				if (m_registry.all_of<fr::RenderDataComponent>(entity))
				{
					auto& renderDataComponent = m_registry.get<fr::RenderDataComponent>(entity);

					// Bounds:
					if (m_registry.all_of<fr::BoundsComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<fr::DestroyRenderDataRenderCommand<gr::Bounds::RenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// MeshPrimitives:
					if (m_registry.all_of<fr::MeshPrimitiveComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<fr::DestroyRenderDataRenderCommand<gr::MeshPrimitive::RenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Mesh Morph Animations:
					if (m_registry.all_of<fr::MeshMorphComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<fr::DestroyRenderDataRenderCommand<gr::MeshPrimitive::MeshMorphRenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Skinning:
					if (m_registry.all_of<fr::SkinningComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<fr::DestroyRenderDataRenderCommand<gr::MeshPrimitive::SkinningRenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Materials:
					if (m_registry.all_of<fr::MaterialInstanceComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<fr::DestroyRenderDataRenderCommand<gr::Material::MaterialInstanceRenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Cameras:
					if (m_registry.all_of<fr::CameraComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<fr::DestroyRenderDataRenderCommand<gr::Camera::RenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Lights:
					if (m_registry.all_of<fr::LightComponent>(entity))
					{
						fr::LightComponent const& lightCmpt = m_registry.get<fr::LightComponent>(entity);
						renderManager->EnqueueRenderCommand<fr::DestroyLightDataRenderCommand>(lightCmpt);
					}

					// ShadowMaps:
					if (m_registry.all_of<fr::ShadowMapComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<fr::DestroyRenderDataRenderCommand<gr::ShadowMap::RenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Now the render data components associated with this entity's use of the RenderDataID are destroyed, 
					// we can destroy the render data objects themselves (or decrement the ref. count if it's a shared ID)
					renderManager->EnqueueRenderCommand<fr::DestroyRenderObjectCommand>(
						renderDataComponent.GetRenderDataID());
				}

				// Manually destroy the relationship, while the component is still active in the registry
				m_registry.get<fr::Relationship>(entity).Destroy();
				
				// Finally, destroy the entity:
				m_registry.destroy(entity);
			}

			m_deferredDeleteQueue.clear();
		}
	}


	void EntityManager::HandleEvents()
	{
		while (HasEvents())
		{
			core::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_type)
			{
			case core::EventManager::EventType::InputToggleConsole:
			{
				// Only enable/disable input processing when the console button is toggled
				if (eventInfo.m_data0.m_dataB)
				{
					m_processInput = !m_processInput;
				}
			}
			break;
			default:
				break;
			}
		}
	}


	void EntityManager::OnBoundsDirty()
	{
		// No lock needed: Event handlers are called from within functions that already hold one

		bool sceneBoundsDirty = false;
		auto dirtySceneBoundsView = m_registry.view<
			fr::BoundsComponent, fr::BoundsComponent::SceneBoundsMarker, DirtyMarker<fr::BoundsComponent>>();
		for (auto entity : dirtySceneBoundsView)
		{
			SEAssert(sceneBoundsDirty == false, "Already found a dirty scene bounds. This should not be possible");
			sceneBoundsDirty = true;
		}

		if (sceneBoundsDirty)
		{
			// Directional light shadows:
			auto directionalLightShadowsView =
				m_registry.view<fr::ShadowMapComponent, fr::LightComponent::DirectionalDeferredMarker>();
			for (auto entity : directionalLightShadowsView)
			{
				m_registry.emplace_or_replace<DirtyMarker<fr::ShadowMapComponent>>(entity);
			}
		}
	}


	void EntityManager::ConfigureRegistry()
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			m_registry.on_construct<DirtyMarker<fr::BoundsComponent>>().connect<&fr::EntityManager::OnBoundsDirty>(*this);
		}
	}


	void EntityManager::UpdateCameraController(double stepTimeMs)
	{
		const entt::entity mainCamera = GetMainCamera();

		const bool isAnimated = HasComponent<fr::AnimationComponent>(mainCamera);

		if (!isAnimated)
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			fr::CameraControlComponent* cameraController = nullptr;
			fr::TransformComponent* camControllerTransform = nullptr;
			bool foundCamController = false;

			auto camControllerView = m_registry.view<fr::CameraControlComponent, fr::TransformComponent>();
			for (entt::entity entity : camControllerView)
			{
				SEAssert(foundCamController == false, "Already found a camera controller. This should not be possible");
				foundCamController = true;

				cameraController = &camControllerView.get<fr::CameraControlComponent>(entity);
				camControllerTransform = &camControllerView.get<fr::TransformComponent>(entity);
			}
			SEAssert(cameraController && camControllerTransform, "Failed to find a camera controller and/or transform");

			fr::CameraControlComponent::Update(
				*cameraController,
				camControllerTransform->GetTransform(),
				GetComponent<fr::CameraComponent>(mainCamera).GetCamera(),
				GetComponent<fr::TransformComponent>(mainCamera).GetTransform(),
				stepTimeMs);
		}
	}


	void EntityManager::UpdateBounds()
	{
		entt::entity sceneBoundsEntity = entt::null;
		{
			// We're only viewing the registry and modifying components in place; Only need a read lock
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto sceneBoundsEntityView = m_registry.view<fr::BoundsComponent, fr::BoundsComponent::SceneBoundsMarker>();
			bool foundSceneBoundsEntity = false;
			for (entt::entity entity : sceneBoundsEntityView)
			{
				SEAssert(foundSceneBoundsEntity == false, 
					"Scene bounds entity already found. This should not be possible");
				foundSceneBoundsEntity = true;

				sceneBoundsEntity = entity;
			}

			// Modify our bounds component in-place:
			m_registry.patch<fr::BoundsComponent>(sceneBoundsEntity, [&](auto& sceneBoundsComponent)
				{
					sceneBoundsComponent = fr::BoundsComponent::Invalid();

					// We must check every MeshConcept in the scene: If even 1 is dirty, we need to recompute everything
					auto meshConceptEntitiesView =
						m_registry.view<fr::Mesh::MeshConceptMarker, fr::BoundsComponent, fr::TransformComponent>();
					for (entt::entity entity : meshConceptEntitiesView)
					{
						fr::BoundsComponent const& boundsComponent =
							meshConceptEntitiesView.get<fr::BoundsComponent>(entity);

						fr::Transform const& meshTransform =
							meshConceptEntitiesView.get<fr::TransformComponent>(entity).GetTransform();

						sceneBoundsComponent.ExpandBounds(
							boundsComponent.GetTransformedAABBBounds(meshTransform.GetGlobalMatrix()),
							sceneBoundsEntity);
					}

					// It is valid for a MeshConcept to have no TransformComponent; We handle this case with its own
					// specialized view
					auto meshWithNoTransformView =
						m_registry.view<fr::Mesh::MeshConceptMarker, fr::BoundsComponent>(entt::exclude<fr::TransformComponent>);
					for (entt::entity entity : meshWithNoTransformView)
					{
						fr::BoundsComponent const& boundsComponent =
							meshWithNoTransformView.get<fr::BoundsComponent>(entity);

						fr::Relationship const& relationship = GetComponent<fr::Relationship>(entity);
						fr::TransformComponent const* transformCmpt =
							relationship.GetFirstInHierarchyAbove<fr::TransformComponent>();
						SEAssert(transformCmpt,
							"Failed to find a TransformComponent in the hierarchy above. This is unexpected");

						sceneBoundsComponent.ExpandBounds(
							boundsComponent.GetTransformedAABBBounds(transformCmpt->GetTransform().GetGlobalMatrix()),
							sceneBoundsEntity);
					}
				});
		}


		// Update "regular" bounds: Mark them as dirty if their transforms have changed
		auto boundsView = m_registry.view<fr::BoundsComponent, fr::Relationship>(
			entt::exclude<fr::BoundsComponent::SceneBoundsMarker>);
		for (entt::entity entity : boundsView)
		{
			fr::BoundsComponent& bounds = boundsView.get<fr::BoundsComponent>(entity);
			fr::Relationship const& relationship = boundsView.get<fr::Relationship>(entity);

			fr::BoundsComponent::UpdateBoundsComponent(*this, bounds, relationship, entity);
		}
	}


	void EntityManager::UpdateAnimationControllers(double stepTimeMs)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Update the animation controllers:
			auto animationControllersView = m_registry.view<fr::AnimationController>();
			for (auto entity : animationControllersView)
			{
				fr::AnimationController& animationController = animationControllersView.get<fr::AnimationController>(entity);
				fr::AnimationController::UpdateAnimationController(animationController, stepTimeMs);
			}

			// Update the individual animation components:
			auto animatedsView = m_registry.view<fr::AnimationComponent, fr::TransformComponent>();
			for (auto entity : animatedsView)
			{
				fr::AnimationComponent& animationComponent = animatedsView.get<fr::AnimationComponent>(entity);
				fr::TransformComponent& transformComponent = animatedsView.get<fr::TransformComponent>(entity);

				fr::AnimationComponent::ApplyAnimation(animationComponent, transformComponent);
			}
		}
	}


	void EntityManager::UpdateAnimations(double stepTimeMs)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Morph animations:
			auto morphMeshesView = 
				m_registry.view<fr::AnimationComponent, fr::MeshMorphComponent, fr::Mesh::MeshConceptMarker>();
			for (auto entity : morphMeshesView)
			{
				fr::AnimationComponent const& animCmpt = morphMeshesView.get<fr::AnimationComponent>(entity);
				fr::MeshMorphComponent& meshAnimCmpt = morphMeshesView.get<fr::MeshMorphComponent>(entity);

				fr::MeshMorphComponent::ApplyAnimation(entity, animCmpt, meshAnimCmpt);
			}

			// Skin animations:
			auto skinnedMeshesView =
				m_registry.view<fr::SkinningComponent, fr::Mesh::MeshConceptMarker>();
			for (auto entity : skinnedMeshesView)
			{
				fr::SkinningComponent& skinCmpt = skinnedMeshesView.get<fr::SkinningComponent>(entity);
				fr::SkinningComponent::UpdateSkinMatrices(*this, entity, skinCmpt, static_cast<float>(stepTimeMs));
			}
		}
	}


	void EntityManager::UpdateTransforms()
	{
		// Use the number of root transforms during the last update 
		static size_t prevNumRootTransforms = 1;

		std::vector<std::future<void>> taskFutures;
		taskFutures.reserve(prevNumRootTransforms);

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto transformComponentsView = m_registry.view<fr::TransformComponent>();
			for (auto entity : transformComponentsView)
			{
				// Find root nodes:
				fr::TransformComponent& transformComponent = transformComponentsView.get<fr::TransformComponent>(entity);
				fr::Transform& node = transformComponent.GetTransform();
				if (node.GetParent() == nullptr)
				{
					fr::TransformComponent::DispatchTransformUpdateThreads(taskFutures, &node);
				}
			}
		}

		prevNumRootTransforms = std::max(1llu, taskFutures.size());

		// Wait for the updates to complete
		for (std::future<void> const& taskFuture : taskFutures)
		{
			taskFuture.wait();
		}
	}


	void EntityManager::UpdateMaterials()
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto materialView = m_registry.view<fr::MaterialInstanceComponent>();
			for (auto entity : materialView)
			{
				fr::MaterialInstanceComponent& matCmpt = materialView.get<fr::MaterialInstanceComponent>(entity);
				if (matCmpt.IsDirty())
				{
					m_registry.emplace_or_replace<DirtyMarker<fr::MaterialInstanceComponent>>(entity);
					matCmpt.ClearDirtyFlag();
				}
			}
		}
	}


	void EntityManager::UpdateLightsAndShadows()
	{
		fr::BoundsComponent const* sceneBounds = GetSceneBounds();
		fr::CameraComponent const* activeSceneCam = &GetComponent<fr::CameraComponent>(GetMainCamera());

		// Add dirty markers to lights and shadows so the render data will be updated
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Ambient lights:
			auto ambientView = m_registry.view<fr::LightComponent, fr::LightComponent::AmbientIBLDeferredMarker>();
			for (auto entity : ambientView)
			{
				fr::LightComponent& lightComponent = ambientView.get<fr::LightComponent>(entity);

				fr::LightComponent::Update(entity, lightComponent, nullptr, nullptr);
			}

			// Punctual lights with (optional) shadows have the same update flow
			auto PunctualLightShadowUpdate = [this](auto& lightView)
				{
					for (auto entity : lightView)
					{
						fr::LightComponent& lightComponent = lightView.get<fr::LightComponent>(entity);

						fr::Camera* shadowCam = nullptr;

						fr::TransformComponent& transformCmpt = lightView.get<fr::TransformComponent>(entity);

						if (m_registry.any_of<fr::ShadowMapComponent::HasShadowMarker>(entity))
						{
							fr::ShadowMapComponent* shadowMapCmpt = &m_registry.get<fr::ShadowMapComponent>(entity);
							SEAssert(shadowMapCmpt, "Failed to find shadow map component");

							fr::CameraComponent* shadowCamCmpt = &m_registry.get<fr::CameraComponent>(entity);
							SEAssert(shadowCamCmpt, "Failed to find shadow camera");

							shadowCam = &shadowCamCmpt->GetCameraForModification();
						}

						fr::LightComponent::Update(entity, lightComponent, &transformCmpt.GetTransform(), shadowCam);
					}
				};

			// Point lights:
			auto const& pointView = 
				m_registry.view<fr::LightComponent, fr::LightComponent::PointDeferredMarker, fr::TransformComponent>();
			PunctualLightShadowUpdate(pointView);

			// Spot lights:
			auto const& spotView =
				m_registry.view<fr::LightComponent, fr::LightComponent::SpotDeferredMarker, fr::TransformComponent>();
			PunctualLightShadowUpdate(spotView);

			// Directional lights:
			auto const& directionalView =
				m_registry.view<fr::LightComponent, fr::LightComponent::DirectionalDeferredMarker, fr::TransformComponent>();
			PunctualLightShadowUpdate(directionalView);


			// Shadows:
			auto shadowsView = 
				m_registry.view<fr::ShadowMapComponent, fr::TransformComponent, fr::LightComponent, fr::CameraComponent>();
			for (auto entity : shadowsView)
			{
				// Force an update if the ShadowMap is already marked as dirty, or its owning light is marked as dirty
				const bool force = m_registry.any_of<DirtyMarker<fr::ShadowMapComponent>>(entity) || 
					m_registry.any_of<DirtyMarker<fr::LightComponent>>(entity);

				fr::TransformComponent& transformCmpt = shadowsView.get<fr::TransformComponent>(entity);
				fr::ShadowMapComponent& shadowMapCmpt = shadowsView.get<fr::ShadowMapComponent>(entity);
				fr::LightComponent const& lightCmpt = shadowsView.get<fr::LightComponent>(entity);
				fr::CameraComponent& shadowCamCmpt = shadowsView.get<fr::CameraComponent>(entity);

				// Update: Attach a dirty marker if anything changed
				fr::ShadowMapComponent::Update(
					entity, shadowMapCmpt, transformCmpt, lightCmpt, shadowCamCmpt, sceneBounds, activeSceneCam, force);
			}
		}
	}


	void EntityManager::UpdateCameras()
	{
		// Check for dirty cameras, or cameras with dirty transforms
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto cameraComponentsView = m_registry.view<fr::CameraComponent>();
			for (auto entity : cameraComponentsView)
			{
				fr::CameraComponent& cameraComponent = cameraComponentsView.get<fr::CameraComponent>(entity);

				fr::Camera& camera = cameraComponent.GetCameraForModification();
				if (camera.IsDirty() || camera.GetTransform()->HasChanged())
				{
					cameraComponent.MarkDirty(*this, entity);
					camera.MarkClean();
				}
			}
		}
	}


	void EntityManager::ShowSceneObjectsImGuiWindow(bool* show)
	{
		if (!*show)
		{
			return;
		}

		static const int windowWidth = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			static_cast<float>(windowWidth) * 0.25f,
			static_cast<float>(windowHeight - k_windowYOffset)),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Scene objects";
		ImGui::Begin(k_panelTitle, show);

		if (ImGui::CollapsingHeader("Cameras", ImGuiTreeNodeFlags_None))
		{
			auto cameraCmptView = m_registry.view<fr::CameraComponent>();

			const entt::entity mainCamEntity = GetMainCamera();

			// Set the initial state of our active index
			static int s_mainCamIdx = 0;
			static bool s_foundFirstMainCam = false;
			if (!s_foundFirstMainCam)
			{
				s_mainCamIdx = 0;
				for (entt::entity entity : cameraCmptView)
				{
					if (entity == mainCamEntity)
					{
						break;
					}
					s_mainCamIdx++;
				}
				s_foundFirstMainCam = true;
			}

			int buttonIdx = 0;
			for (entt::entity entity : cameraCmptView)
			{
				// Display a radio button on the same line as our camera header:
				const bool pressed = ImGui::RadioButton(
					std::format("##{}", static_cast<uint32_t>(entity)).c_str(),
					&s_mainCamIdx,
					buttonIdx++);
				ImGui::SameLine();
				fr::CameraComponent::ShowImGuiWindow(*this, entity);
				ImGui::Separator();

				// Update the main camera:
				if (pressed)
				{
					SetMainCamera(entity);
				}
			}
		} // "Cameras"

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Camera controller", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			entt::entity mainCam = GetMainCamera();

			auto camControllerView = m_registry.view<fr::CameraControlComponent>();
			for (entt::entity entity : camControllerView)
			{
				fr::CameraControlComponent::ShowImGuiWindow(*this, entity, mainCam);
			}
			ImGui::Unindent();
		} // "Camera controller"

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			auto animControllerView = m_registry.view<fr::AnimationController>();
			for (entt::entity entity : animControllerView)
			{
				fr::AnimationController::ShowImGuiWindow(*this, entity);
				ImGui::Separator();
			}

			ImGui::Unindent();
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			auto meshView = m_registry.view<fr::Mesh::MeshConceptMarker>();
			for (entt::entity entity : meshView)
			{
				fr::Mesh::ShowImGuiWindow(*this, entity);
				ImGui::Separator();
			}

			ImGui::Unindent();
		} // "Meshes"

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			auto materialView = m_registry.view<fr::MaterialInstanceComponent>();
			for (entt::entity entity : materialView)
			{
				fr::MaterialInstanceComponent::ShowImGuiWindow(*this, entity);
				ImGui::Separator();
			}

			ImGui::Unindent();
		} // "Materials"

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			if (ImGui::CollapsingHeader("Ambient Lights", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();

				const entt::entity currentActiveAmbient = GetActiveAmbientLight();

				auto ambientLightView =
					m_registry.view<fr::LightComponent, fr::LightComponent::AmbientIBLDeferredMarker>();

				// Find the index of the currently active ambient light:
				int activeAmbientLightIndex = 0;
				for (entt::entity entity : ambientLightView)
				{
					if (entity == currentActiveAmbient)
					{
						break;
					}
					activeAmbientLightIndex++;
				}

				// Display radio buttons next to each ambient light:
				int buttonIdx = 0;				
				for (entt::entity entity : ambientLightView)
				{
					if (ImGui::RadioButton(
						std::format("##{}", static_cast<uint32_t>(entity)).c_str(),
						&activeAmbientLightIndex,
						buttonIdx++))
					{
						SetActiveAmbientLight(entity);
					}
					ImGui::SameLine();
					fr::LightComponent::ShowImGuiWindow(*this, entity);
				}
				ImGui::Unindent();
			}

			if (ImGui::CollapsingHeader("Directional Lights", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				auto directionalLightView = m_registry.view<fr::LightComponent, fr::LightComponent::DirectionalDeferredMarker>();
				for (entt::entity entity : directionalLightView)
				{
					fr::LightComponent::ShowImGuiWindow(*this, entity);
				}
				ImGui::Unindent();
			}

			if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				auto pointLightView = m_registry.view<fr::LightComponent, fr::LightComponent::PointDeferredMarker>();
				for (entt::entity entity : pointLightView)
				{
					fr::LightComponent::ShowImGuiWindow(*this, entity);
				}
				ImGui::Unindent();
			}

			if (ImGui::CollapsingHeader("Spot Lights", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				auto spotLightView = m_registry.view<fr::LightComponent, fr::LightComponent::SpotDeferredMarker>();
				for (entt::entity entity : spotLightView)
				{
					fr::LightComponent::ShowImGuiWindow(*this, entity);
				}
				ImGui::Unindent();
			}

			ImGui::Unindent();
		} // "Lights"

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Shadow maps", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			auto shadowMapView = m_registry.view<fr::ShadowMapComponent>();
			for (auto entity : shadowMapView)
			{
				fr::ShadowMapComponent::ShowImGuiWindow(*this, entity);
			}

			ImGui::Unindent();
		} // "Shadow maps"

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Bounds", ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			auto boundsView = m_registry.view<fr::BoundsComponent>();
			for (auto entity : boundsView)
			{
				fr::BoundsComponent::ShowImGuiWindow(*this, entity, true);
			}

			ImGui::Unindent();
		} // "Bounds"

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Render data IDs", ImGuiTreeNodeFlags_None))
		{
			std::vector<fr::RenderDataComponent const*> renderDataComponents;

			auto renderDataView = m_registry.view<fr::RenderDataComponent>();
			for (auto entity : renderDataView)
			{
				fr::RenderDataComponent const& renderDataCmpt = renderDataView.get<fr::RenderDataComponent>(entity);

				renderDataComponents.emplace_back(&renderDataCmpt);
			}

			fr::RenderDataComponent::ShowImGuiWindow(renderDataComponents);
		} // "Render data IDs"

		//ImGui::Separator();

		ImGui::End();
	}


	void EntityManager::ShowSceneTransformImGuiWindow(bool* show)
	{
		if (!*show)
		{
			return;
		}

		// Build a list of root nodes to pass to the Transform window to process:
		static size_t s_numRootNodes = 16;
		std::vector<fr::Transform*> rootNodes;
		rootNodes.reserve(s_numRootNodes);

		std::vector<entt::entity> rootNodeEntities;
		auto transformCmptView = m_registry.view<fr::TransformComponent>();
		for (entt::entity entity : transformCmptView)
		{
			fr::TransformComponent& transformCmpt = transformCmptView.get<fr::TransformComponent>(entity);
			if (transformCmpt.GetTransform().GetParent() == nullptr)
			{
				rootNodeEntities.emplace_back(entity);
			}
		}
		s_numRootNodes = std::max(s_numRootNodes, rootNodes.size());

		fr::Transform::ShowImGuiWindow(*this, rootNodeEntities, show);
	}


	void EntityManager::ShowImGuiEntityComponentDebugHelper(
		std::vector<entt::entity> rootEntities, bool expandAll, bool expandChangeTriggered)
	{
		for (entt::entity curRoot : rootEntities)
		{
			ShowImGuiEntityComponentDebugHelper(curRoot, expandAll, expandChangeTriggered);

			ImGui::Separator();
		}
	}


	void EntityManager::ShowImGuiEntityComponentDebugHelper(
		entt::entity entity, bool expandAll, bool expandChangeTriggered)
	{
		auto ShowEntityControls = [this](entt::entity entity)
			{
				if (ImGui::Button("Delete"))
				{
					fr::Relationship const& entityRelationship = GetComponent<fr::Relationship>(entity);

					// This is executed on the render thread, so we register children for deletion first, then
					// parents, so we don't risk having orphans between frames
					std::vector<entt::entity> descendents = entityRelationship.GetAllDescendents();
					for (size_t i = descendents.size(); i > 0; i--)
					{
						RegisterEntityForDelete(descendents[i - 1]);
					}

					RegisterEntityForDelete(entity);
				}
			};

		constexpr float k_indentSize = 16.f;

		if (expandChangeTriggered)
		{
			ImGui::SetNextItemOpen(expandAll);
		}

		struct NodeState
		{
			entt::entity m_entity;
			uint32_t m_depth;
		};
		std::stack<NodeState> nodes;
		nodes.push(NodeState{.m_entity = entity, .m_depth = 1});

		while (!nodes.empty())
		{
			NodeState curNodeState = nodes.top();
			nodes.pop();

			// Add children for next iteration:
			fr::Relationship const& curNodeRelationship = GetComponent<fr::Relationship>(curNodeState.m_entity);

			const entt::entity firstChild = curNodeRelationship.GetFirstChild();
			if (firstChild != entt::null)
			{
				entt::entity curChild = firstChild;
				do
				{
					nodes.push(NodeState{ curChild, curNodeState.m_depth + 1 });

					fr::Relationship const& curRelationship = GetComponent<fr::Relationship>(curChild);

					curChild = curRelationship.GetNext();
				} while (curChild != firstChild);
			}

			ImGui::Indent(k_indentSize* curNodeState.m_depth);

			if (expandChangeTriggered)
			{
				ImGui::SetNextItemOpen(expandAll);
			}

			fr::NameComponent const& nameCmpt = GetComponent<fr::NameComponent>(curNodeState.m_entity);

			if (ImGui::TreeNode(std::format("Entity {}: \"{}\"",
				static_cast<uint32_t>(curNodeState.m_entity),
				nameCmpt.GetName()).c_str()))
			{
				ImGui::SameLine();
				ShowEntityControls(curNodeState.m_entity);

				ImGui::Indent();

				// List the component types:
				{
					std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

					for (auto&& curr : m_registry.storage())
					{
						entt::id_type cid = curr.first;
						auto& storage = curr.second;
						entt::type_info ctype = storage.type();

						if (storage.contains(curNodeState.m_entity))
						{
							ImGui::BulletText(std::format("{}", ctype.name()).c_str());
						}
					}
				}

				ImGui::Unindent();

				ImGui::TreePop();
			}


			ImGui::Unindent(k_indentSize* curNodeState.m_depth);
		}
	}


	void EntityManager::ShowImGuiEntityComponentDebug(bool* show)
	{
		if (!*show)
		{
			return;
		}

		static const int windowWidth = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		static const int windowHeight = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			windowWidth * k_windowWidthPercentage,
			static_cast<float>(windowHeight) - k_windowYOffset),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Node Hierarchy";
		ImGui::Begin(k_panelTitle, show);

		static bool s_expandAll = false;
		bool expandChangeTriggered = false;
		if (ImGui::Button(s_expandAll ? "Hide all" : "Expand all"))
		{
			s_expandAll = !s_expandAll;
			expandChangeTriggered = true;
		}

		// Build a list of root entities, and sort them for readability
		std::vector<entt::entity> sortedRootEntities;
		for (auto entityTuple : m_registry.storage<entt::entity>().each())
		{
			const entt::entity curEntity = std::get<entt::entity>(entityTuple);
			fr::Relationship const& curRelationship = m_registry.get<fr::Relationship>(curEntity);

			if (!curRelationship.HasParent())
			{
				sortedRootEntities.emplace_back(curEntity);
			}
		}
		std::sort(sortedRootEntities.begin(), sortedRootEntities.end());

		// Call the recursive helper:
		ShowImGuiEntityComponentDebugHelper(sortedRootEntities, s_expandAll, expandChangeTriggered);

		ImGui::End();
	}
}