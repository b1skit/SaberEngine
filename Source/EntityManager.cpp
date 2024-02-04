// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "CameraComponent.h"
#include "Config.h"
#include "CoreEngine.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "MaterialInstanceComponent.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "CameraControlComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"
#include "ShadowMapComponent.h"


namespace fr
{
	EntityManager* EntityManager::Get()
	{
		static std::unique_ptr<fr::EntityManager> instance = std::make_unique<fr::EntityManager>(PrivateCTORTag{});
		return instance.get();
	}


	EntityManager::EntityManager(PrivateCTORTag)
		: m_processInput(false)
	{
		// Handle this during construction before anything can interact with the registry
		ConfigureRegistry();
	}


	void EntityManager::Startup()
	{
		LOG("EntityManager starting...");

		// Event subscriptions:
		en::EventManager::Get()->Subscribe(en::EventManager::EventType::InputToggleConsole, this);

		// Create a scene bounds entity:
		fr::BoundsComponent::CreateSceneBoundsConcept(*this);

		// Create an Ambient light:
		fr::LightComponent::CreateDeferredAmbientLightConcept(*this, fr::SceneManager::GetSceneData()->GetIBLTexture());

		// Add a player object to the scene:
		entt::entity mainCameraEntity = GetMainCamera();
		fr::NameComponent const& mainCameraName = GetComponent<fr::NameComponent>(mainCameraEntity);

		fr::CameraControlComponent::CreateCameraControlConcept(*this, mainCameraEntity);
		LOG("Created PlayerObject using \"%s\"", mainCameraName.GetName().c_str());
		m_processInput = true;


		// Push render updates to ensure new data is available for the first frame
		EnqueueRenderUpdates();
	}


	void EntityManager::Shutdown()
	{
		LOG("EntityManager shutting down...");

		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

			// Add all entities to the deferred delete queue
			for (auto entity : m_registry.storage<entt::entity>())
			{
				RegisterEntityForDelete(entity);
			}
		}

		ExecuteDeferredDeletions();

		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);
			m_registry.clear();
		}
	}


	void EntityManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		HandleEvents();

		// Handle interaction (player input, physics, animation, etc)
		if (m_processInput)
		{
			UpdateCameraController(stepTimeMs);
		}
		
		// Update the scene state:
		UpdateTransforms();
		UpdateSceneBounds();
		UpdateMaterials();
		UpdateLightsAndShadows();
		UpdateCameras();

		ExecuteDeferredDeletions();
	}


	template<typename T, typename RenderDataType>
	void EntityManager::EnqueueRenderUpdateHelper()
	{
		re::RenderManager* renderManager = re::RenderManager::Get();

		auto componentsView = m_registry.view<T, DirtyMarker<T>, gr::RenderDataComponent, fr::NameComponent>();
		for (auto entity : componentsView)
		{
			gr::RenderDataComponent const& renderDataComponent = componentsView.get<gr::RenderDataComponent>(entity);
			fr::NameComponent const& nameComponent = componentsView.get<fr::NameComponent>(entity);

			T const& component = componentsView.get<T>(entity);

			renderManager->EnqueueRenderCommand<gr::UpdateRenderDataRenderCommand<RenderDataType>>(
				renderDataComponent.GetRenderDataID(),
				T::CreateRenderData(component, nameComponent));

			m_registry.erase<DirtyMarker<T>>(entity);
		}
	}


	void EntityManager::EnqueueRenderUpdates()
	{
		re::RenderManager* renderManager = re::RenderManager::Get();

		// ECS_CONVERSION TODO: Move each of these isolated tasks to a thread
		// -> Use entt::organizer

		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

			// Register new render objects:
			auto newRenderableEntitiesView = 
				m_registry.view<gr::RenderDataComponent, gr::RenderDataComponent::NewRegistrationMarker>();
			for (auto entity : newRenderableEntitiesView)
			{
				// Enqueue a command to create a new object on the render thread:
				auto& renderDataComponent = newRenderableEntitiesView.get<gr::RenderDataComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::RegisterRenderObjectCommand>(renderDataComponent);
				
				m_registry.erase<gr::RenderDataComponent::NewRegistrationMarker>(entity);
			}

			// Initialize new Transforms associated with a RenderDataComponent:
			auto newTransformComponentsView = 
				m_registry.view<fr::TransformComponent, fr::TransformComponent::NewIDMarker, gr::RenderDataComponent>();
			for (auto entity : newTransformComponentsView)
			{
				fr::TransformComponent& transformComponent =
					newTransformComponentsView.get<fr::TransformComponent>(entity);

				renderManager->EnqueueRenderCommand<fr::UpdateTransformDataRenderCommand>(transformComponent);

				m_registry.erase<fr::TransformComponent::NewIDMarker>(entity);
			}

			// Clear the NewIDMarker from any remaining TransformComponents not associated with a gr::RenderDataComponent
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
			// Note: We only send Transforms associated with RenderDataComponents to the render thread
			auto transformComponentsView = m_registry.view<fr::TransformComponent, gr::RenderDataComponent>();
			for (auto entity : transformComponentsView)
			{
				fr::TransformComponent& transformComponent = 
					transformComponentsView.get<fr::TransformComponent>(entity);

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
				gr::RenderDataComponent>();
			for (auto entity : newMainCameraView)
			{
				gr::RenderDataComponent const& renderDataComponent =
					newMainCameraView.get<gr::RenderDataComponent>(entity);

				renderManager->EnqueueRenderCommand<fr::SetActiveCameraRenderCommand>(
					renderDataComponent.GetRenderDataID(), renderDataComponent.GetTransformID());

				m_registry.erase<fr::CameraComponent::NewMainCameraMarker>(entity);
			}

			EnqueueRenderUpdateHelper<fr::BoundsComponent, gr::Bounds::RenderData>();
			EnqueueRenderUpdateHelper<fr::MeshPrimitiveComponent, gr::MeshPrimitive::RenderData>();
			EnqueueRenderUpdateHelper<fr::MaterialInstanceComponent, gr::Material::MaterialInstanceData>();
			EnqueueRenderUpdateHelper<fr::CameraComponent, gr::Camera::RenderData>();

			// Update dirty render data components that touch the GraphicsSystems directly:

			// Lights:
			auto lightComponentsView = m_registry.view<
				fr::LightComponent, DirtyMarker<fr::LightComponent>, gr::RenderDataComponent, fr::NameComponent>();
			for (auto entity : lightComponentsView)
			{
				fr::NameComponent const& nameComponent = lightComponentsView.get<fr::NameComponent>(entity);
				fr::LightComponent const& lightComponent = lightComponentsView.get<fr::LightComponent>(entity);
				renderManager->EnqueueRenderCommand<fr::UpdateLightDataRenderCommand>(nameComponent, lightComponent);

				m_registry.erase<DirtyMarker<fr::LightComponent>>(entity);
			}

			// Shadows:
			auto shadowMapComponentsView =
				m_registry.view<fr::ShadowMapComponent, DirtyMarker<fr::ShadowMapComponent>, gr::RenderDataComponent>();
			for (auto entity : shadowMapComponentsView)
			{
				fr::NameComponent const& nameComponent = lightComponentsView.get<fr::NameComponent>(entity);
				fr::ShadowMapComponent const& shadowMapComponent = 
					shadowMapComponentsView.get<fr::ShadowMapComponent>(entity);

				renderManager->EnqueueRenderCommand<fr::UpdateShadowMapDataRenderCommand>(
					nameComponent, shadowMapComponent);

				m_registry.erase<DirtyMarker<fr::ShadowMapComponent>>(entity);
			}
		}
	}


	fr::BoundsComponent const* EntityManager::GetSceneBounds() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

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


	void EntityManager::SetMainCamera(entt::entity camera)
	{
		SEAssert(camera != entt::null && HasComponent<fr::CameraComponent>(camera),
			"Entity does not have a valid camera component");

		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			SetMainCameraInternal(camera);
		}

	}


	void EntityManager::SetMainCameraInternal(entt::entity newMainCamera)
	{
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
		fr::TransformComponent* camControllerTransformCmpt = nullptr;
		entt::entity camController = entt::null;
		bool foundCamController = false;
		auto camControllerView = m_registry.view<fr::CameraControlComponent>();
		for (entt::entity entity : camControllerView)
		{
			SEAssert(!foundCamController, "Already found camera controller. This shouldn't be possible");
			foundCamController = true;

			camControllerTransformCmpt = &m_registry.get<fr::TransformComponent>(entity);
		}

		// No point trying to set a camera if the camera controller doesn't exist yet
		if (camControllerTransformCmpt)
		{
			fr::TransformComponent& currentCamTransformCmpt = m_registry.get<fr::TransformComponent>(currentMainCamera);
			fr::TransformComponent& newCamTransformCmpt = m_registry.get<fr::TransformComponent>(newMainCamera);

			fr::CameraControlComponent::SetCamera(
				*camControllerTransformCmpt, &currentCamTransformCmpt, newCamTransformCmpt);
		}
	}


	entt::entity EntityManager::GetMainCamera() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);
			return GetMainCameraInternal();
		}
	}


	entt::entity EntityManager::GetMainCameraInternal() const
	{
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


	entt::entity EntityManager::CreateEntity(std::string const& name)
	{
		return CreateEntity(name.c_str());
	}


	entt::entity EntityManager::CreateEntity(char const* name)
	{
		entt::entity newEntity = entt::null;
		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);
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
				if (m_registry.all_of<gr::RenderDataComponent>(entity))
				{
					auto& renderDataComponent = m_registry.get<gr::RenderDataComponent>(entity);

					// Bounds:
					if (m_registry.all_of<fr::BoundsComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<gr::Bounds::RenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// MeshPrimitives:
					if (m_registry.all_of<fr::MeshPrimitiveComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<gr::MeshPrimitive::RenderData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Materials:
					if (m_registry.all_of<fr::MaterialInstanceComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<gr::Material::MaterialInstanceData>>(
							renderDataComponent.GetRenderDataID());
					}

					// Cameras:
					if (m_registry.all_of<fr::CameraComponent>(entity))
					{
						renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<gr::Camera::RenderData>>(
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
						fr::ShadowMapComponent const& shadowMapCmpt = m_registry.get<fr::ShadowMapComponent>(entity);
						renderManager->EnqueueRenderCommand<fr::DestroyShadowMapDataRenderCommand>(shadowMapCmpt);
					}

					// Now the render data components associated with this entity's use of the RenderDataID are destroyed, 
					// we can destroy the render data objects themselves (or decrement the ref. count if it's a shared ID)
					renderManager->EnqueueRenderCommand<gr::DestroyRenderObjectCommand>(
						renderDataComponent.GetRenderDataID());
				}

				
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
			en::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_type)
			{
			case en::EventManager::EventType::InputToggleConsole:
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
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			m_registry.on_construct<DirtyMarker<fr::BoundsComponent>>().connect<&fr::EntityManager::OnBoundsDirty>(*this);
		}
	}


	void EntityManager::UpdateCameraController(double stepTimeMs)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			fr::CameraComponent* cameraComponent = nullptr;
			fr::TransformComponent* cameraTransform = nullptr;
			bool foundMainCamera = false;
			auto mainCameraView = m_registry.view<
				fr::CameraComponent, fr::CameraComponent::MainCameraMarker, fr::TransformComponent>();
			for (entt::entity mainCamEntity : mainCameraView)
			{
				SEAssert(foundMainCamera == false, "Already found a main camera. This should not be possible");
				foundMainCamera = true;

				cameraComponent = &mainCameraView.get<fr::CameraComponent>(mainCamEntity);
				cameraTransform = &mainCameraView.get<fr::TransformComponent>(mainCamEntity);
			}
			SEAssert(cameraComponent && cameraTransform, "Failed to find main CameraComponent or TransformComponent");

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
				cameraComponent->GetCamera(), 
				cameraTransform->GetTransform(),
				stepTimeMs);
		}
	}


	void EntityManager::UpdateSceneBounds()
	{
		entt::entity sceneBoundsEntity = entt::null;
		bool sceneBoundsChanged = false;
		{
			// We're only viewing the registry and modifying components in place; Only need a read lock
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			auto sceneBoundsEntityView = m_registry.view<fr::BoundsComponent, fr::BoundsComponent::SceneBoundsMarker>();
			bool foundSceneBoundsEntity = false;
			for (entt::entity entity : sceneBoundsEntityView)
			{
				SEAssert(foundSceneBoundsEntity == false, 
					"Scene bounds entity already found. This should not be possible");
				foundSceneBoundsEntity = true;

				sceneBoundsEntity = entity;
			}

			// Copy the current bounds so we can detect if it changes
			const fr::BoundsComponent prevBounds = sceneBoundsEntityView.get<fr::BoundsComponent>(sceneBoundsEntity);

			// Modify our bounds component in-place:
			m_registry.patch<fr::BoundsComponent>(sceneBoundsEntity, [&](auto& sceneBoundsComponent)
				{
					auto meshConceptEntitiesView = 
						m_registry.view<fr::Mesh::MeshConceptMarker, fr::BoundsComponent, fr::TransformComponent>();

					// We must process every MeshConcept in the scene, even if it hasn't changed since we last checked
					bool seenMeshConceptEntity = false;
					for (auto meshConceptEntity : meshConceptEntitiesView)
					{
						fr::Transform const& meshTransform =
							meshConceptEntitiesView.get<fr::TransformComponent>(meshConceptEntity).GetTransform();

						// Copy the first bounds we see; we'll expand it to encompass all Bounds in the scene
						if (!seenMeshConceptEntity)
						{
							sceneBoundsComponent = meshConceptEntitiesView.get<fr::BoundsComponent>(meshConceptEntity);
							seenMeshConceptEntity = true;
						}

						fr::BoundsComponent const& boundsComponent =
							meshConceptEntitiesView.get<fr::BoundsComponent>(meshConceptEntity);

						sceneBoundsComponent.ExpandBounds(
							boundsComponent.GetTransformedAABBBounds(meshTransform.GetGlobalMatrix()));
					}
				});

			fr::BoundsComponent const& newSceneBounds = sceneBoundsEntityView.get<fr::BoundsComponent>(sceneBoundsEntity);
			sceneBoundsChanged = (newSceneBounds != prevBounds);
		}

		// Mark the scene bounds as dirty; This will trigger updates to anything that depends on it (e.g. shadow cam 
		// frustums)
		if (sceneBoundsChanged)
		{
			EmplaceOrReplaceComponent<DirtyMarker<fr::BoundsComponent>>(sceneBoundsEntity);
		}
	}


	void EntityManager::UpdateTransforms()
	{
		// Use the number of root transforms during the last update 
		static size_t prevNumRootTransforms = 1;

		std::vector<std::future<void>> taskFutures;
		taskFutures.reserve(prevNumRootTransforms);

		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			auto transformComponentsView = m_registry.view<fr::TransformComponent>();
			for (auto entity : transformComponentsView)
			{
				fr::TransformComponent& transformComponent =
					transformComponentsView.get<fr::TransformComponent>(entity);

				// Find root nodes:
				fr::Transform& node = transformComponent.GetTransform();
				if (node.GetParent() == nullptr)
				{
					fr::TransformComponent::DispatchTransformUpdateThreads(taskFutures, &node);
				}
			}

			prevNumRootTransforms = std::max(1llu, taskFutures.size());

			// Wait for the updates to complete
			for (std::future<void> const& taskFuture : taskFutures)
			{
				taskFuture.wait();
			}
		}
	}


	void EntityManager::UpdateMaterials()
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

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

		// Add dirty markers to lights and shadows so the render data will be updated
		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

			// Ambient lights:
			auto ambientView = m_registry.view<fr::LightComponent, fr::LightComponent::AmbientIBLDeferredMarker>();
			for (auto entity : ambientView)
			{
				fr::LightComponent& lightComponent = ambientView.get<fr::LightComponent>(entity);

				if (fr::LightComponent::Update(lightComponent, nullptr, nullptr))
				{
					m_registry.emplace_or_replace<DirtyMarker<fr::LightComponent>>(entity);
				}
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

						// Update: Attach a dirty marker if anything changed
						if (fr::LightComponent::Update(lightComponent, &transformCmpt.GetTransform(), shadowCam))
						{
							m_registry.emplace_or_replace<DirtyMarker<fr::LightComponent>>(entity);
						}
					}
				};

			// Point lights:
			auto const& pointView = 
				m_registry.view<fr::LightComponent, fr::LightComponent::PointDeferredMarker, fr::TransformComponent>();
			PunctualLightShadowUpdate(pointView);

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

				fr::TransformComponent const& transformCmpt = shadowsView.get<fr::TransformComponent>(entity);
				fr::ShadowMapComponent& shadowMapCmpt = shadowsView.get<fr::ShadowMapComponent>(entity);
				fr::LightComponent const& lightCmpt = shadowsView.get<fr::LightComponent>(entity);
				fr::CameraComponent& shadowCamCmpt = shadowsView.get<fr::CameraComponent>(entity);

				// Update: Attach a dirty marker if anything changed
				if (fr::ShadowMapComponent::Update(
					shadowMapCmpt, transformCmpt, lightCmpt, shadowCamCmpt, sceneBounds, force))
				{
					m_registry.emplace_or_replace<DirtyMarker<fr::ShadowMapComponent>>(entity);
				}
			}
		}
	}


	void EntityManager::UpdateCameras()
	{
		// Check for dirty cameras, or cameras with dirty transforms
		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

			auto cameraComponentsView = m_registry.view<fr::CameraComponent>();
			for (auto entity : cameraComponentsView)
			{
				fr::CameraComponent& cameraComponent = cameraComponentsView.get<fr::CameraComponent>(entity);

				fr::Camera& camera = cameraComponent.GetCameraForModification();
				if (camera.IsDirty() || camera.GetTransform()->HasChanged())
				{
					m_registry.emplace_or_replace<DirtyMarker<fr::CameraComponent>>(entity);
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

		static const int windowWidth = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
		static const int windowHeight = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);
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

			auto ambientLightView = m_registry.view<fr::LightComponent, fr::LightComponent::AmbientIBLDeferredMarker>();
			for (entt::entity entity : ambientLightView)
			{
				fr::LightComponent::ShowImGuiWindow(*this, entity);
			}

			if (ImGui::CollapsingHeader("Directional Lights:", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				auto directionalLightView = m_registry.view<fr::LightComponent, fr::LightComponent::DirectionalDeferredMarker>();
				for (entt::entity entity : directionalLightView)
				{
					fr::LightComponent::ShowImGuiWindow(*this, entity);
				}
				ImGui::Unindent();
			}

			if (ImGui::CollapsingHeader("Point Lights:", ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				auto pointLightView = m_registry.view<fr::LightComponent, fr::LightComponent::PointDeferredMarker>();
				for (entt::entity entity : pointLightView)
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


		if (ImGui::CollapsingHeader("Render data IDs", ImGuiTreeNodeFlags_None))
		{
			std::vector<gr::RenderDataComponent const*> renderDataComponents;

			auto renderDataView = m_registry.view<gr::RenderDataComponent>();
			for (auto entity : renderDataView)
			{
				gr::RenderDataComponent const& renderDataCmpt = renderDataView.get<gr::RenderDataComponent>(entity);

				renderDataComponents.emplace_back(&renderDataCmpt);
			}

			gr::RenderDataComponent::ShowImGuiWindow(renderDataComponents);
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

		static const int windowWidth = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
		static const int windowHeight = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			static_cast<float>(windowWidth) * 0.25f,
			static_cast<float>(windowHeight - k_windowYOffset)),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Scene Transforms";
		ImGui::Begin(k_panelTitle, show);

		
		static size_t s_numRootNodes = 16;
		std::vector<fr::Transform*> rootNodes;
		rootNodes.reserve(s_numRootNodes);

		auto transformCmptView = m_registry.view<fr::TransformComponent>();
		for (entt::entity entity : transformCmptView)
		{
			fr::TransformComponent& transformCmpt = transformCmptView.get<fr::TransformComponent>(entity);
			if (transformCmpt.GetTransform().GetParent() == nullptr)
			{
				rootNodes.emplace_back(&transformCmpt.GetTransform());
			}
		}
		s_numRootNodes = std::max(s_numRootNodes, rootNodes.size());

		fr::Transform::ShowImGuiWindow(rootNodes, show);

		ImGui::End();
	}


	void EntityManager::ShowImGuiEntityComponentDebug(bool* show)
	{
		if (!*show)
		{
			return;
		}

		static const int windowWidth = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
		static const int windowHeight = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);
		constexpr float k_windowYOffset = 64.f;
		constexpr float k_windowWidthPercentage = 0.25f;

		ImGui::SetNextWindowSize(ImVec2(
			windowWidth * k_windowWidthPercentage,
			static_cast<float>(windowHeight) - k_windowYOffset),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(0, k_windowYOffset), ImGuiCond_FirstUseEver, ImVec2(0, 0));

		constexpr char const* k_panelTitle = "Entity/Component View";
		ImGui::Begin(k_panelTitle, show);

		static bool s_expandAll = false;
		bool expandChangeTriggered = false;
		if (ImGui::Button(s_expandAll ? "Hide all" : "Expand all"))
		{
			s_expandAll = !s_expandAll;
			expandChangeTriggered = true;
		}

		auto ShowEntityControls = [this](entt::entity entity)
			{
				if (ImGui::Button("Delete"))
				{
					RegisterEntityForDelete(entity);
				}
			};

		// Iterate over all entities:
		for (auto entityTuple : m_registry.storage<entt::entity>().each())
		{
			entt::entity entity = std::get<entt::entity>(entityTuple);

			fr::NameComponent const& nameCmpt = m_registry.get<fr::NameComponent>(entity);

			if (expandChangeTriggered)
			{
				ImGui::SetNextItemOpen(s_expandAll);
			}
			if (ImGui::TreeNode(std::format("Entity {} \"{}\"",
				static_cast<uint32_t>(entity),
				nameCmpt.GetName()).c_str()))
			{
				ImGui::Indent();

				for (auto&& curr : m_registry.storage())
				{
					entt::id_type cid = curr.first;
					auto& storage = curr.second;
					entt::type_info ctype = storage.type();

					if (storage.contains(entity))
					{
						ImGui::BulletText(std::format("{}", ctype.name()).c_str());
					}
				}

				ShowEntityControls(entity);

				ImGui::Unindent();
				ImGui::TreePop();
			}

			ImGui::Separator();
		}

		ImGui::End();
	}
}