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
#include "ShadowMapComponent.h"
#include "SkinningComponent.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/ProfilingMarkers.h"

#include "Core/Definitions/ConfigKeys.h"
#include "Core/Definitions/EventKeys.h"

#include "Renderer/RenderManager.h"


namespace
{
	constexpr size_t k_entityCommandBufferSize = 1024;
}

namespace pr
{
	EntityManager* EntityManager::Get()
	{
		static std::unique_ptr<pr::EntityManager> instance = std::make_unique<pr::EntityManager>(PrivateCTORTag{});
		return instance.get();
	}


	EntityManager::EntityManager(PrivateCTORTag)
		: m_entityCommands(k_entityCommandBufferSize)
	{
		// Handle this during construction before anything can interact with the registry
		ConfigureRegistry();
	}


	void EntityManager::Startup()
	{
		LOG("EntityManager starting...");

		// Event subscriptions:
		core::EventManager::Get()->Subscribe(eventkey::SceneResetRequest, this);

		// Process entity commands issued during scene loading:
		ProcessEntityCommands();
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
		UpdateCameraController(stepTimeMs);

		// Update the scene state:
		UpdateAnimationControllers(stepTimeMs);  // Modifies Transforms

		UpdateTransforms(); // Transforms are immutable after this point
		
		UpdateAnimations(stepTimeMs);
		UpdateBounds();
		UpdateMaterials();
		UpdateLightsAndShadows();
		UpdateCameras();

		ExecuteDeferredDeletions();
	}


	void EntityManager::ProcessEntityCommands()
	{
		SEBeginCPUEvent("EntityManager::ProcessEntityCommands");
		{
			m_entityCommands.SwapBuffers();
			m_entityCommands.Execute();
		}
		SEEndCPUEvent();
	}


	template<typename RenderDataType, typename CmptType, typename... OtherCmpts>
	void EntityManager::EnqueueRenderUpdateHelper()
	{
		gr::RenderManager* renderManager = gr::RenderManager::Get();

		auto componentsView = m_registry.view<pr::RenderDataComponent, DirtyMarker<CmptType>, CmptType, OtherCmpts...>();
		for (auto entity : componentsView)
		{
			pr::RenderDataComponent const& renderDataComponent = componentsView.get<pr::RenderDataComponent>(entity);

			CmptType const& component = componentsView.get<CmptType>(entity);

			renderManager->EnqueueRenderCommand<pr::UpdateRenderDataRenderCommand<RenderDataType>>(
				renderDataComponent.GetRenderDataID(),
				CmptType::CreateRenderData(entity, component));

			m_registry.erase<DirtyMarker<CmptType>>(entity);
		}
	}


	void EntityManager::EnqueueRenderUpdates()
	{
		gr::RenderManager* renderManager = gr::RenderManager::Get();

		// ECS_CONVERSION TODO: Move each of these isolated tasks to a thread
		// -> Use entt::organizer

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Register new render objects:
			auto newRenderableEntitiesView = 
				m_registry.view<pr::RenderDataComponent, pr::RenderDataComponent::NewRegistrationMarker>();
			for (auto entity : newRenderableEntitiesView)
			{
				// Enqueue a command to create a new object on the render thread:
				auto& renderDataComponent = newRenderableEntitiesView.get<pr::RenderDataComponent>(entity);

				renderManager->EnqueueRenderCommand<pr::RegisterRenderObjectCommand>(renderDataComponent);
				
				m_registry.erase<pr::RenderDataComponent::NewRegistrationMarker>(entity);
			}

			// Initialize new Transforms associated with a RenderDataComponent:
			auto newTransformComponentsView = 
				m_registry.view<pr::TransformComponent, pr::TransformComponent::NewIDMarker, pr::RenderDataComponent>();
			for (auto entity : newTransformComponentsView)
			{
				pr::TransformComponent& transformComponent =
					newTransformComponentsView.get<pr::TransformComponent>(entity);

				renderManager->EnqueueRenderCommand<pr::UpdateTransformDataRenderCommand>(transformComponent);

				m_registry.erase<pr::TransformComponent::NewIDMarker>(entity);
			}

			// Clear the NewIDMarker from any remaining TransformComponents not associated with a pr::RenderDataComponent
			auto remainingNewTransformsView = 
				m_registry.view<pr::TransformComponent, pr::TransformComponent::NewIDMarker>();
			for (auto entity : remainingNewTransformsView)
			{
				pr::TransformComponent& transformComponent =
					remainingNewTransformsView.get<pr::TransformComponent>(entity);

				m_registry.erase<pr::TransformComponent::NewIDMarker>(entity);
			}

			// Update dirty render data components:
			// ------------------------------------

			// Transforms:
			auto transformCmptsView = m_registry.view<pr::TransformComponent>();
			for (auto entity : transformCmptsView)
			{
				pr::TransformComponent& transformComponent = transformCmptsView.get<pr::TransformComponent>(entity);

				if (transformComponent.GetTransform().HasChanged())
				{
					renderManager->EnqueueRenderCommand<pr::UpdateTransformDataRenderCommand>(transformComponent);
					transformComponent.GetTransform().ClearHasChangedFlag();
				}
			}

			// Handle camera changes:
			auto newMainCameraView = m_registry.view<
				pr::CameraComponent,
				pr::CameraComponent::MainCameraMarker,
				pr::CameraComponent::NewMainCameraMarker,
				pr::RenderDataComponent>();
			for (auto entity : newMainCameraView)
			{
				pr::RenderDataComponent const& renderDataComponent =
					newMainCameraView.get<pr::RenderDataComponent>(entity);

				renderManager->EnqueueRenderCommand<pr::SetActiveCameraRenderCommand>(
					renderDataComponent.GetRenderDataID(), renderDataComponent.GetTransformID());

				m_registry.erase<pr::CameraComponent::NewMainCameraMarker>(entity);
			}

			EnqueueRenderUpdateHelper<gr::Bounds::RenderData, pr::BoundsComponent>();
			EnqueueRenderUpdateHelper<gr::MeshPrimitive::RenderData, pr::MeshPrimitiveComponent>();
			EnqueueRenderUpdateHelper<gr::Material::MaterialInstanceRenderData, pr::MaterialInstanceComponent>();
			EnqueueRenderUpdateHelper<gr::Camera::RenderData, pr::CameraComponent>();
			EnqueueRenderUpdateHelper<gr::MeshPrimitive::MeshMorphRenderData, pr::MeshMorphComponent, 
				pr::Mesh::MeshConceptMarker, pr::AnimationComponent>();
			EnqueueRenderUpdateHelper<gr::MeshPrimitive::SkinningRenderData, pr::SkinningComponent>();

			// Lights:
			auto lightComponentsView = m_registry.view<
				pr::RenderDataComponent, pr::NameComponent, DirtyMarker<pr::LightComponent>, pr::LightComponent>();
			for (auto entity : lightComponentsView)
			{
				pr::NameComponent const& nameComponent = lightComponentsView.get<pr::NameComponent>(entity);
				pr::LightComponent const& lightComponent = lightComponentsView.get<pr::LightComponent>(entity);
				renderManager->EnqueueRenderCommand<pr::UpdateLightDataRenderCommand>(nameComponent, lightComponent);

				m_registry.erase<DirtyMarker<pr::LightComponent>>(entity);
			}

			// Shadows:
			EnqueueRenderUpdateHelper<gr::ShadowMap::RenderData, pr::ShadowMapComponent>();
		}
	}


	pr::BoundsComponent const* EntityManager::GetSceneBounds() const
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto sceneBoundsEntityView = m_registry.view<pr::BoundsComponent, pr::BoundsComponent::SceneBoundsMarker>();
			SEAssert(sceneBoundsEntityView.front() == sceneBoundsEntityView.back(),
				"A unique scene bounds entity must exist");

			const entt::entity sceneBoundsEntity = sceneBoundsEntityView.front();
			if (sceneBoundsEntity != entt::null)
			{
				return &sceneBoundsEntityView.get<pr::BoundsComponent>(sceneBoundsEntity);
			}
			else
			{
				return nullptr;
			}			
		}
	}


	void EntityManager::SetMainCamera(entt::entity newMainCamera)
	{
		SEAssert(newMainCamera != entt::null && HasComponent<pr::CameraComponent>(newMainCamera),
			"Entity does not have a valid camera component");

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			entt::entity currentMainCamera = entt::null;
			bool foundCurrentMainCamera = false;
			auto currentMainCameraView = m_registry.view<pr::CameraComponent::MainCameraMarker>();
			for (auto entity : currentMainCameraView)
			{
				SEAssert(foundCurrentMainCamera == false, "Already found a main camera. This should not be possible");
				foundCurrentMainCamera = true;

				currentMainCamera = entity;

				m_registry.erase<pr::CameraComponent::MainCameraMarker>(entity);

				// Deactivate the current main camera:
				pr::CameraComponent& cameraComponent = m_registry.get<pr::CameraComponent>(entity);
				cameraComponent.GetCameraForModification().SetActive(false);

				// If the main camera was added during the current frame, ensure we don't end up with 2 new camera markers
				if (m_registry.any_of<pr::CameraComponent::NewMainCameraMarker>(entity))
				{
					m_registry.erase<pr::CameraComponent::NewMainCameraMarker>(entity);
				}
			}

			m_registry.emplace_or_replace<pr::CameraComponent::MainCameraMarker>(newMainCamera);
			m_registry.emplace_or_replace<pr::CameraComponent::NewMainCameraMarker>(newMainCamera);

			// Activate the new main camera:
			pr::CameraComponent& cameraComponent = m_registry.get<pr::CameraComponent>(newMainCamera);
			cameraComponent.GetCameraForModification().SetActive(true);

			// Find and update the camera controller:
			entt::entity camController = entt::null;
			auto camControllerView = m_registry.view<pr::CameraControlComponent>();
			for (entt::entity entity : camControllerView)
			{
				SEAssert(camController == entt::null, "Already found camera controller. This shouldn't be possible");
				camController = entity;
			}

			if (camController != entt::null) // No point trying to set a camera if the camera controller doesn't exist yet
			{
				// Animated cameras cannot be controlled by a camera controller
				entt::entity camControllerTarget = entt::null;
				if (!m_registry.any_of<pr::AnimationComponent>(newMainCamera))
				{
					camControllerTarget = newMainCamera;
				}
				pr::CameraControlComponent::SetCamera(camController, currentMainCamera, camControllerTarget);
			}
		}
	}


	entt::entity EntityManager::GetMainCamera() const
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
			
			entt::entity mainCamEntity = entt::null;

			bool foundCurrentMainCamera = false;
			auto currentMainCameraView = m_registry.view<pr::CameraComponent::MainCameraMarker>();
			for (auto entity : currentMainCameraView)
			{
				SEAssert(foundCurrentMainCamera == false, "Already found a main camera. This should not be possible");
				foundCurrentMainCamera = true;

				mainCamEntity = entity;
			}

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
				pr::LightComponent& prevActiveLightComponent = GetComponent<pr::LightComponent>(prevActiveAmbient);

				SEAssert(prevActiveLightComponent.GetLight().GetType() == pr::Light::Type::AmbientIBL,
					"Light component is not the correct type");

				pr::Light::TypeProperties prevLightTypeProperties =
					prevActiveLightComponent.GetLight().GetLightTypeProperties(pr::Light::Type::AmbientIBL);

				SEAssert(prevLightTypeProperties.m_ambient.m_isActive,
					"Ambient light is not active. This should not be possible");

				prevLightTypeProperties.m_ambient.m_isActive = false;

				// This will mark the light as dirty, and trigger an update
				prevActiveLightComponent.GetLight().SetLightTypeProperties(
					pr::Light::Type::AmbientIBL, &prevLightTypeProperties.m_ambient);

				RemoveComponent<pr::LightComponent::IsActiveAmbientDeferredMarker>(prevActiveAmbient);
			}

			// Promote the new light to the active one:
			pr::LightComponent& lightComponent = GetComponent<pr::LightComponent>(ambientLight);

			SEAssert(lightComponent.GetLight().GetType() == pr::Light::Type::AmbientIBL,
				"Light component is not the correct type");

			// Update the active flag:
			pr::Light::TypeProperties currentLightTypeProperties =
				lightComponent.GetLight().GetLightTypeProperties(pr::Light::Type::AmbientIBL);

			SEAssert(!currentLightTypeProperties.m_ambient.m_isActive,
				"Ambient light is already active. This is harmless, but unexpected");

			currentLightTypeProperties.m_ambient.m_isActive = true;

			// This will mark the light as dirty, and trigger an update
			lightComponent.GetLight().SetLightTypeProperties(
				pr::Light::Type::AmbientIBL, &currentLightTypeProperties.m_ambient);

			// Mark the new light as the active light:
			EmplaceComponent<pr::LightComponent::IsActiveAmbientDeferredMarker>(ambientLight);
		}
	}


	entt::entity EntityManager::GetActiveAmbientLight() const
	{
		entt::entity activeAmbient = entt::null;
		bool foundCurrentActiveAmbient = false;

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto currentActiveAmbient = m_registry.view<pr::LightComponent::IsActiveAmbientDeferredMarker>();
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


	void EntityManager::Reset()
	{
		LOG("EntityManager: Resetting registry");

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Register all entities for delete
			for (auto entityTuple : m_registry.storage<entt::entity>().each())
			{
				const entt::entity curEntity = std::get<entt::entity>(entityTuple);

				RegisterEntityForDelete(curEntity);
			}

			ExecuteDeferredDeletions();

			m_registry.clear();
		}

		// Note: There's a potential ordering issue here, where we'll receive a reset event and clear the registry, and
		// then possibly immediately create new entities from ProcessEntityCommands() registered before the reset event.
		// There are arguements either way about which is preferable, for now just leaving this comment for awareness
	}


	entt::entity EntityManager::CreateEntity(std::string_view name)
	{
		SEAssert(name.data()[name.size()] == '\0', "std::string_view must be null-terminated for EntityManager usage");
		
		entt::entity newEntity = entt::null;
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
			newEntity = m_registry.create();
		}

		pr::NameComponent::AttachNameComponent(*this, newEntity, name.data());
		pr::Relationship::AttachRelationshipComponent(*this, newEntity);

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
		SEBeginCPUEvent("EntityManager::ExecuteDeferredDeletions");

		gr::RenderManager* renderManager = gr::RenderManager::Get();

		if (!m_deferredDeleteQueue.empty())
		{
			std::scoped_lock lock(m_registeryMutex, m_deferredDeleteQueueMutex);

			for (entt::entity entity : m_deferredDeleteQueue)
			{
				// If the entity has a RenderDataComponent, we must enqueue delete commands for the render thread
				if (m_registry.all_of<pr::RenderDataComponent>(entity))
				{
					auto& renderDataComponent = m_registry.get<pr::RenderDataComponent>(entity);

					// Bounds:
					if (m_registry.all_of<pr::BoundsComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<pr::DestroyRenderDataRenderCommand<gr::Bounds::RenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// MeshPrimitives:
					if (m_registry.all_of<pr::MeshPrimitiveComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<pr::DestroyRenderDataRenderCommand<gr::MeshPrimitive::RenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Mesh Morph Animations:
					if (m_registry.all_of<pr::MeshMorphComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<pr::DestroyRenderDataRenderCommand<gr::MeshPrimitive::MeshMorphRenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Skinning:
					if (m_registry.all_of<pr::SkinningComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<pr::DestroyRenderDataRenderCommand<gr::MeshPrimitive::SkinningRenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Materials:
					if (m_registry.all_of<pr::MaterialInstanceComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<pr::DestroyRenderDataRenderCommand<gr::Material::MaterialInstanceRenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Cameras:
					if (m_registry.all_of<pr::CameraComponent>(entity))
					{
						const entt::entity mainCamera = GetMainCamera();

						if (entity == mainCamera)
						{
							renderManager->EnqueueRenderCommand<pr::SetActiveCameraRenderCommand>(
								gr::k_invalidRenderDataID, gr::k_invalidTransformID);
						}

						renderManager->EnqueueRenderCommand<pr::DestroyRenderDataRenderCommand<gr::Camera::RenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Lights:
					if (m_registry.all_of<pr::LightComponent>(entity))
					{
						pr::LightComponent const& lightCmpt = m_registry.get<pr::LightComponent>(entity);
						renderManager->EnqueueRenderCommand<pr::DestroyLightDataRenderCommand>(lightCmpt);
					}

					// ShadowMaps:
					if (m_registry.all_of<pr::ShadowMapComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<pr::DestroyRenderDataRenderCommand<gr::ShadowMap::RenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Now the render data components associated with this entity's use of the RenderDataID are destroyed, 
					// we can destroy the render data objects themselves (or decrement the ref. count if it's a shared ID)
					renderManager->EnqueueRenderCommand<pr::DestroyRenderObjectCommand>(
						renderDataComponent.GetRenderDataID());
				}

				// Manually destroy the relationship, while the component is still active in the registry
				m_registry.get<pr::Relationship>(entity).Destroy();
				
				// Finally, destroy the entity:
				m_registry.destroy(entity);
			}

			m_deferredDeleteQueue.clear();
		}

		SEEndCPUEvent();
	}


	void EntityManager::HandleEvents()
	{
		SEBeginCPUEvent("EntityManager::HandleEvents");
		while (HasEvents())
		{
			core::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_eventKey)
			{
			case eventkey::SceneResetRequest:
			{
				Reset();
			}
			break;
			default:
				break;
			}
		}
		SEEndCPUEvent();
	}


	void EntityManager::OnBoundsDirty()
	{
		// No lock needed: Event handlers are called from within functions that already hold one

		bool sceneBoundsDirty = false;
		auto dirtySceneBoundsView = m_registry.view<
			pr::BoundsComponent, pr::BoundsComponent::SceneBoundsMarker, DirtyMarker<pr::BoundsComponent>>();
		for (auto entity : dirtySceneBoundsView)
		{
			SEAssert(sceneBoundsDirty == false, "Already found a dirty scene bounds. This should not be possible");
			sceneBoundsDirty = true;
		}

		if (sceneBoundsDirty)
		{
			// Directional light shadows:
			auto directionalLightShadowsView =
				m_registry.view<pr::ShadowMapComponent, pr::LightComponent::DirectionalDeferredMarker>();
			for (auto entity : directionalLightShadowsView)
			{
				m_registry.emplace_or_replace<DirtyMarker<pr::ShadowMapComponent>>(entity);
			}
		}
	}


	void EntityManager::ConfigureRegistry()
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			m_registry.on_construct<DirtyMarker<pr::BoundsComponent>>().connect<&pr::EntityManager::OnBoundsDirty>(*this);
		}
	}


	void EntityManager::UpdateCameraController(double stepTimeMs)
	{
		SEBeginCPUEvent("EntityManager::UpdateCameraController");

		const entt::entity mainCamera = GetMainCamera();

		if (mainCamera != entt::null &&
			!HasComponent<pr::AnimationComponent>(mainCamera))
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			pr::CameraControlComponent* cameraController = nullptr;
			pr::TransformComponent* camControllerTransform = nullptr;
			bool foundCamController = false;

			auto camControllerView = m_registry.view<pr::CameraControlComponent, pr::TransformComponent>();
			for (entt::entity entity : camControllerView)
			{
				SEAssert(foundCamController == false, "Already found a camera controller. This should not be possible");
				foundCamController = true;

				cameraController = &camControllerView.get<pr::CameraControlComponent>(entity);
				camControllerTransform = &camControllerView.get<pr::TransformComponent>(entity);
			}
			SEAssert(cameraController && camControllerTransform, "Failed to find a camera controller and/or transform");

			pr::CameraControlComponent::Update(
				*cameraController,
				camControllerTransform->GetTransform(),
				GetComponent<pr::CameraComponent>(mainCamera).GetCamera(),
				GetComponent<pr::TransformComponent>(mainCamera).GetTransform(),
				stepTimeMs);
		}
		SEEndCPUEvent();
	}


	void EntityManager::UpdateBounds()
	{
		SEBeginCPUEvent("EntityManager::UpdateBounds");
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Update "regular" bounds: Mark them as dirty if their transforms have changed
			auto boundsView = m_registry.view<pr::BoundsComponent, pr::Relationship>(
				entt::exclude<pr::BoundsComponent::SceneBoundsMarker>);
			for (entt::entity entity : boundsView)
			{
				pr::BoundsComponent& bounds = boundsView.get<pr::BoundsComponent>(entity);
				pr::Relationship const& relationship = boundsView.get<pr::Relationship>(entity);

				pr::BoundsComponent::UpdateBoundsComponent(*this, bounds, relationship, entity);
			}

			// Find the scene bounds entity:
			// TODO: Cache this entity by subscribing to create/delete callbacks for the SceneBoundsMarker
			entt::entity sceneBoundsEntity = entt::null;
			auto sceneBoundsEntityView = m_registry.view<pr::BoundsComponent, pr::BoundsComponent::SceneBoundsMarker>();
			bool foundSceneBoundsEntity = false;
			for (entt::entity entity : sceneBoundsEntityView)
			{
				SEAssert(foundSceneBoundsEntity == false,
					"Scene bounds entity already found. This should not be possible");
				foundSceneBoundsEntity = true;

				sceneBoundsEntity = entity;
			}

			if (foundSceneBoundsEntity) // Might be a null entity (e.g. if we just reset the scene)
			{
				// If any bounds are dirty, we must update the scene bounds:
				if (EntityExists<pr::BoundsComponent, DirtyMarker<pr::BoundsComponent>>())
				{
					// Modify our bounds component in-place:
					m_registry.patch<pr::BoundsComponent>(sceneBoundsEntity, [&](auto& sceneBoundsComponent)
						{
							sceneBoundsComponent = pr::BoundsComponent::Invalid();

							bool foundOtherBounds = false;

							auto boundsView = m_registry.view<pr::BoundsComponent, pr::Relationship>(
								entt::exclude<pr::BoundsComponent::SceneBoundsMarker>);
							for (entt::entity entity : boundsView)
							{
								pr::BoundsComponent const& boundsComponent = boundsView.get<pr::BoundsComponent>(entity);

								// Only need to recompute on Bounds with no parents (as they're recursively recomputed
								// on children)
								// TODO: It would be more logical to add the scene bounds as the encapsulating bounds
								// for otherwise root Bounds
								if (boundsComponent.GetEncapsulatingBoundsEntity() == entt::null)
								{
									pr::Relationship const& relationship = boundsView.get<pr::Relationship>(entity);

									pr::TransformComponent const* transformCmpt =
										relationship.GetFirstInHierarchyAbove<pr::TransformComponent>();
									SEAssert(transformCmpt,
										"Failed to find a TransformComponent in the hierarchy above. This is unexpected");

									sceneBoundsComponent.ExpandBounds(
										boundsComponent.GetTransformedAABBBounds(
											transformCmpt->GetTransform().GetGlobalMatrix()),
										sceneBoundsEntity);

									foundOtherBounds = true;
								}
							}

							// If there are no other bounds, we set the scene bounds to zero (preventing it from getting
							// stuck at the last size it saw another bounds)
							if (!foundOtherBounds)
							{
								sceneBoundsComponent = pr::BoundsComponent::Zero();
								pr::BoundsComponent::MarkDirty(sceneBoundsEntity);
							}
						});
				}				
			}
		}
		SEEndCPUEvent();
	}


	void EntityManager::UpdateAnimationControllers(double stepTimeMs)
	{
		SEBeginCPUEvent("EntityManager::UpdateAnimationControllers");
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Update the animation controllers:
			auto animationControllersView = m_registry.view<pr::AnimationController>();
			for (auto entity : animationControllersView)
			{
				pr::AnimationController& animationController = animationControllersView.get<pr::AnimationController>(entity);
				pr::AnimationController::UpdateAnimationController(animationController, stepTimeMs);
			}

			// Update the individual animation components:
			auto animatedsView = m_registry.view<pr::AnimationComponent, pr::TransformComponent>();
			for (auto entity : animatedsView)
			{
				pr::AnimationComponent& animationComponent = animatedsView.get<pr::AnimationComponent>(entity);
				pr::TransformComponent& transformComponent = animatedsView.get<pr::TransformComponent>(entity);

				pr::AnimationComponent::ApplyAnimation(animationComponent, transformComponent);
			}
		}
		SEEndCPUEvent();
	}


	void EntityManager::UpdateAnimations(double stepTimeMs)
	{
		SEBeginCPUEvent("EntityManager::UpdateAnimations");
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Morph animations:
			auto morphMeshesView = 
				m_registry.view<pr::AnimationComponent, pr::MeshMorphComponent, pr::Mesh::MeshConceptMarker>();
			for (auto entity : morphMeshesView)
			{
				pr::AnimationComponent const& animCmpt = morphMeshesView.get<pr::AnimationComponent>(entity);
				pr::MeshMorphComponent& meshAnimCmpt = morphMeshesView.get<pr::MeshMorphComponent>(entity);

				pr::MeshMorphComponent::ApplyAnimation(entity, animCmpt, meshAnimCmpt);
			}

			// Skin animations:
			auto skinnedMeshesView =
				m_registry.view<pr::SkinningComponent, pr::Mesh::MeshConceptMarker>();
			for (auto entity : skinnedMeshesView)
			{
				pr::SkinningComponent& skinCmpt = skinnedMeshesView.get<pr::SkinningComponent>(entity);
				pr::SkinningComponent::UpdateSkinMatrices(*this, entity, skinCmpt, static_cast<float>(stepTimeMs));
			}
		}
		SEEndCPUEvent();
	}


	void EntityManager::UpdateTransforms()
	{
		SEBeginCPUEvent("EntityManager::UpdateTransforms");

		// Use the number of root transforms during the last update 
		static size_t prevNumRootTransforms = 1;

		std::vector<std::future<void>> taskFutures;
		taskFutures.reserve(prevNumRootTransforms);

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto transformComponentsView = m_registry.view<pr::TransformComponent>();
			for (auto entity : transformComponentsView)
			{
				// Find root nodes:
				pr::TransformComponent& transformComponent = transformComponentsView.get<pr::TransformComponent>(entity);
				pr::Transform& node = transformComponent.GetTransform();
				if (node.GetParent() == nullptr)
				{
					pr::TransformComponent::DispatchTransformUpdateThreads(taskFutures, &node);
				}
			}
		}

		prevNumRootTransforms = std::max(1llu, taskFutures.size());

		// Wait for the updates to complete
		for (std::future<void> const& taskFuture : taskFutures)
		{
			taskFuture.wait();
		}

		SEEndCPUEvent();
	}


	void EntityManager::UpdateMaterials()
	{
		SEBeginCPUEvent("EntityManager::UpdateMaterials");
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto materialView = m_registry.view<pr::MaterialInstanceComponent>();
			for (auto entity : materialView)
			{
				pr::MaterialInstanceComponent& matCmpt = materialView.get<pr::MaterialInstanceComponent>(entity);
				if (matCmpt.IsDirty())
				{
					m_registry.emplace_or_replace<DirtyMarker<pr::MaterialInstanceComponent>>(entity);
					matCmpt.ClearDirtyFlag();
				}
			}
		}
		SEEndCPUEvent();
	}


	void EntityManager::UpdateLightsAndShadows()
	{
		SEBeginCPUEvent("EntityManager::UpdateLightsAndShadows");

		const entt::entity mainCameraEntity = GetMainCamera();
		if (mainCameraEntity == entt::null)
		{
			SEEndCPUEvent();
			return;
		}

		pr::BoundsComponent const* sceneBounds = GetSceneBounds();
		pr::CameraComponent const* activeSceneCam = &GetComponent<pr::CameraComponent>(mainCameraEntity);

		// Add dirty markers to lights and shadows so the render data will be updated
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			// Ambient lights:
			auto ambientView = m_registry.view<pr::LightComponent, pr::LightComponent::AmbientIBLDeferredMarker>();
			for (auto entity : ambientView)
			{
				pr::LightComponent& lightComponent = ambientView.get<pr::LightComponent>(entity);

				pr::LightComponent::Update(entity, lightComponent, nullptr, nullptr);
			}

			// Punctual lights with (optional) shadows have the same update flow
			auto PunctualLightShadowUpdate = [this](auto& lightView)
				{
					for (auto entity : lightView)
					{
						pr::LightComponent& lightComponent = lightView.get<pr::LightComponent>(entity);

						pr::Camera* shadowCam = nullptr;

						pr::TransformComponent& transformCmpt = lightView.get<pr::TransformComponent>(entity);

						if (m_registry.any_of<pr::ShadowMapComponent::HasShadowMarker>(entity))
						{
							pr::ShadowMapComponent* shadowMapCmpt = &m_registry.get<pr::ShadowMapComponent>(entity);
							SEAssert(shadowMapCmpt, "Failed to find shadow map component");

							pr::CameraComponent* shadowCamCmpt = &m_registry.get<pr::CameraComponent>(entity);
							SEAssert(shadowCamCmpt, "Failed to find shadow camera");

							shadowCam = &shadowCamCmpt->GetCameraForModification();
						}

						pr::LightComponent::Update(entity, lightComponent, &transformCmpt.GetTransform(), shadowCam);
					}
				};

			// Point lights:
			auto const& pointView = 
				m_registry.view<pr::LightComponent, pr::LightComponent::PointDeferredMarker, pr::TransformComponent>();
			PunctualLightShadowUpdate(pointView);

			// Spot lights:
			auto const& spotView =
				m_registry.view<pr::LightComponent, pr::LightComponent::SpotDeferredMarker, pr::TransformComponent>();
			PunctualLightShadowUpdate(spotView);

			// Directional lights:
			auto const& directionalView =
				m_registry.view<pr::LightComponent, pr::LightComponent::DirectionalDeferredMarker, pr::TransformComponent>();
			PunctualLightShadowUpdate(directionalView);


			// Shadows:
			auto shadowsView = 
				m_registry.view<pr::ShadowMapComponent, pr::TransformComponent, pr::LightComponent, pr::CameraComponent>();
			for (auto entity : shadowsView)
			{
				// Force an update if the ShadowMap is already marked as dirty, or its owning light is marked as dirty
				const bool force = m_registry.any_of<DirtyMarker<pr::ShadowMapComponent>>(entity) || 
					m_registry.any_of<DirtyMarker<pr::LightComponent>>(entity);

				pr::TransformComponent& transformCmpt = shadowsView.get<pr::TransformComponent>(entity);
				pr::ShadowMapComponent& shadowMapCmpt = shadowsView.get<pr::ShadowMapComponent>(entity);
				pr::LightComponent const& lightCmpt = shadowsView.get<pr::LightComponent>(entity);
				pr::CameraComponent& shadowCamCmpt = shadowsView.get<pr::CameraComponent>(entity);

				// Update: Attach a dirty marker if anything changed
				pr::ShadowMapComponent::Update(
					entity, shadowMapCmpt, transformCmpt, lightCmpt, shadowCamCmpt, sceneBounds, activeSceneCam, force);
			}
		}

		SEEndCPUEvent();
	}


	void EntityManager::UpdateCameras()
	{
		SEBeginCPUEvent("EntityManager::UpdateCameras");

		// Check for dirty cameras, or cameras with dirty transforms
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto cameraComponentsView = m_registry.view<pr::CameraComponent>();
			for (auto entity : cameraComponentsView)
			{
				pr::CameraComponent& cameraComponent = cameraComponentsView.get<pr::CameraComponent>(entity);

				pr::Camera& camera = cameraComponent.GetCameraForModification();
				if (camera.IsDirty() || camera.GetTransform()->HasChanged())
				{
					cameraComponent.MarkDirty(*this, entity);
					camera.MarkClean();
				}
			}
		}
		SEEndCPUEvent();
	}


	void EntityManager::ShowSceneObjectsImGuiWindow(bool* show)
	{
		if (!*show)
		{
			return;
		}

		SEBeginCPUEvent("EntityManager::ShowSceneObjectsImGuiWindow");

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
		
		if (ImGui::Begin(k_panelTitle, show))
		{
			if (ImGui::CollapsingHeader("Cameras", ImGuiTreeNodeFlags_None))
			{
				auto cameraCmptView = m_registry.view<pr::CameraComponent>();

				const entt::entity mainCamEntity = GetMainCamera();

				// Find the index of the main camera:
				int mainCamIdx = 0;
				for (entt::entity entity : cameraCmptView)
				{
					if (entity == mainCamEntity)
					{
						break;
					}
					mainCamIdx++;
				}

				int buttonIdx = 0;
				for (entt::entity entity : cameraCmptView)
				{
					// Display a radio button on the same line as our camera header:
					const bool pressed = ImGui::RadioButton(
						std::format("##{}", static_cast<uint32_t>(entity)).c_str(),
						&mainCamIdx,
						buttonIdx++);
					ImGui::SameLine();
					pr::CameraComponent::ShowImGuiWindow(*this, entity);
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

				auto camControllerView = m_registry.view<pr::CameraControlComponent>();
				for (entt::entity entity : camControllerView)
				{
					pr::CameraControlComponent::ShowImGuiWindow(*this, entity, mainCam);
				}
				ImGui::Unindent();
			} // "Camera controller"

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();

				auto animControllerView = m_registry.view<pr::AnimationController>();
				for (entt::entity entity : animControllerView)
				{
					pr::AnimationController::ShowImGuiWindow(*this, entity);
					ImGui::Separator();
				}

				ImGui::Unindent();
			}

			ImGui::Separator();

			auto meshView = m_registry.view<pr::Mesh::MeshConceptMarker>();
			if (ImGui::CollapsingHeader(std::format("Meshes ({})", meshView.size()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();

				for (entt::entity entity : meshView)
				{
					pr::Mesh::ShowImGuiWindow(*this, entity);
					ImGui::Separator();
				}

				ImGui::Unindent();
			} // "Meshes"

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();

				auto materialView = m_registry.view<pr::MaterialInstanceComponent>();
				for (entt::entity entity : materialView)
				{
					pr::MaterialInstanceComponent::ShowImGuiWindow(*this, entity);
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
						m_registry.view<pr::LightComponent, pr::LightComponent::AmbientIBLDeferredMarker>();

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
						pr::LightComponent::ShowImGuiWindow(*this, entity);
					}
					ImGui::Unindent();
				}

				if (ImGui::CollapsingHeader("Directional Lights", ImGuiTreeNodeFlags_None))
				{
					ImGui::Indent();
					auto directionalLightView = m_registry.view<pr::LightComponent, pr::LightComponent::DirectionalDeferredMarker>();
					for (entt::entity entity : directionalLightView)
					{
						pr::LightComponent::ShowImGuiWindow(*this, entity);
					}
					ImGui::Unindent();
				}

				if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_None))
				{
					ImGui::Indent();
					auto pointLightView = m_registry.view<pr::LightComponent, pr::LightComponent::PointDeferredMarker>();
					for (entt::entity entity : pointLightView)
					{
						pr::LightComponent::ShowImGuiWindow(*this, entity);
					}
					ImGui::Unindent();
				}

				if (ImGui::CollapsingHeader("Spot Lights", ImGuiTreeNodeFlags_None))
				{
					ImGui::Indent();
					auto spotLightView = m_registry.view<pr::LightComponent, pr::LightComponent::SpotDeferredMarker>();
					for (entt::entity entity : spotLightView)
					{
						pr::LightComponent::ShowImGuiWindow(*this, entity);
					}
					ImGui::Unindent();
				}

				ImGui::Unindent();
			} // "Lights"

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Shadow maps", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();

				auto shadowMapView = m_registry.view<pr::ShadowMapComponent>();
				for (auto entity : shadowMapView)
				{
					pr::ShadowMapComponent::ShowImGuiWindow(*this, entity);
				}

				ImGui::Unindent();
			} // "Shadow maps"

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Bounds", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();

				auto boundsView = m_registry.view<pr::BoundsComponent>();
				for (auto entity : boundsView)
				{
					pr::BoundsComponent::ShowImGuiWindow(*this, entity, true);
				}

				ImGui::Unindent();
			} // "Bounds"

			ImGui::Separator();

			if (ImGui::CollapsingHeader("Render data IDs", ImGuiTreeNodeFlags_None))
			{
				std::vector<pr::RenderDataComponent const*> renderDataComponents;

				auto renderDataView = m_registry.view<pr::RenderDataComponent>();
				for (auto entity : renderDataView)
				{
					pr::RenderDataComponent const& renderDataCmpt = renderDataView.get<pr::RenderDataComponent>(entity);

					renderDataComponents.emplace_back(&renderDataCmpt);
				}

				pr::RenderDataComponent::ShowImGuiWindow(renderDataComponents);
			} // "Render data IDs"

			//ImGui::Separator();


		}

		ImGui::End();

		SEEndCPUEvent(); // EntityManager::ShowSceneObjectsImGuiWindow
	}


	void EntityManager::ShowSceneTransformImGuiWindow(bool* show)
	{
		if (!*show)
		{
			return;
		}

		SEBeginCPUEvent("EntityManager::ShowSceneTransformImGuiWindow");

		// Build a list of root nodes to pass to the Transform window to process:
		SEBeginCPUEvent("EntityManager::ShowSceneTransformImGuiWindow: Build root nodes list");
		static size_t s_numRootNodes = 16;
		std::vector<pr::Transform*> rootNodes;
		rootNodes.reserve(s_numRootNodes);

		std::vector<entt::entity> rootNodeEntities;
		auto transformCmptView = m_registry.view<pr::TransformComponent>();
		for (entt::entity entity : transformCmptView)
		{
			pr::TransformComponent& transformCmpt = transformCmptView.get<pr::TransformComponent>(entity);
			if (transformCmpt.GetTransform().GetParent() == nullptr)
			{
				rootNodeEntities.emplace_back(entity);
			}
		}
		s_numRootNodes = std::max(s_numRootNodes, rootNodes.size());
		SEEndCPUEvent(); // EntityManager::ShowSceneTransformImGuiWindow: Build root nodes list

		pr::Transform::ShowImGuiWindow(*this, rootNodeEntities, show);

		SEEndCPUEvent(); // EntityManager::ShowSceneTransformImGuiWindow
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
					pr::Relationship const& entityRelationship = GetComponent<pr::Relationship>(entity);

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
			pr::Relationship const& curNodeRelationship = GetComponent<pr::Relationship>(curNodeState.m_entity);

			const entt::entity firstChild = curNodeRelationship.GetFirstChild();
			if (firstChild != entt::null)
			{
				entt::entity curChild = firstChild;
				do
				{
					nodes.push(NodeState{ curChild, curNodeState.m_depth + 1 });

					pr::Relationship const& curRelationship = GetComponent<pr::Relationship>(curChild);

					curChild = curRelationship.GetNext();
				} while (curChild != firstChild);
			}

			ImGui::Indent(k_indentSize* curNodeState.m_depth);

			if (expandChangeTriggered)
			{
				ImGui::SetNextItemOpen(expandAll);
			}

			pr::NameComponent const& nameCmpt = GetComponent<pr::NameComponent>(curNodeState.m_entity);

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

		SEBeginCPUEvent("EntityManager::ShowImGuiEntityComponentDebug");
		
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
		if (ImGui::Begin(k_panelTitle, show))
		{
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
				pr::Relationship const& curRelationship = m_registry.get<pr::Relationship>(curEntity);

				if (!curRelationship.HasParent())
				{
					sortedRootEntities.emplace_back(curEntity);
				}
			}
			std::sort(sortedRootEntities.begin(), sortedRootEntities.end());

			// Call the recursive helper:
			ShowImGuiEntityComponentDebugHelper(sortedRootEntities, s_expandAll, expandChangeTriggered);
		}
		ImGui::End();

		SEEndCPUEvent();
	}
}