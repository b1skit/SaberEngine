// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "CameraComponent.h"
#include "CoreEngine.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "MaterialComponent.h"
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
		fr::LightComponent::CreateDeferredAmbientLightConcept(*this, en::SceneManager::GetSceneData()->GetIBLTexture());

		// Add a player object to the scene:
		char const* mainCamName = nullptr;
		entt::entity cameraEntity = entt::null;
		{
			std::shared_lock<std::shared_mutex> registryReadLock(m_registeryMutex);

			bool foundMainCamera = false;
			auto mainCameraView = 
				m_registry.view<fr::CameraComponent, fr::CameraComponent::MainCameraMarker, fr::Relationship, fr::NameComponent>();
			for (entt::entity mainCamEntity : mainCameraView)
			{
				SEAssert("Already found a main camera. This should not be possible", foundMainCamera == false);
				foundMainCamera = true;

				cameraEntity = mainCamEntity;

				fr::NameComponent const& camName = mainCameraView.get<fr::NameComponent>(mainCamEntity);
				mainCamName = camName.GetName().c_str();
			}
			SEAssert("Failed to find the main camera", foundMainCamera);
		}

		fr::CameraControlComponent::CreatePlayerObjectConcept(*this, cameraEntity);
		LOG("Created PlayerObject using \"%s\"", mainCamName);
		m_processInput = true;


		// Push render updates to ensure new data is available for the first frame
		EnqueueRenderUpdates();
	}


	void EntityManager::Shutdown()
	{
		LOG("EntityManager shutting down...");

		// Issue render commands to destroy render data:
		re::RenderManager* renderManager = re::RenderManager::Get();

		{
			std::shared_lock<std::shared_mutex> registryReadLock(m_registeryMutex);

			// Destroy any render data components:
			auto renderDataEntitiesView = m_registry.view<gr::RenderDataComponent>();
			for (auto entity : renderDataEntitiesView)
			{
				auto& renderDataComponent = renderDataEntitiesView.get<gr::RenderDataComponent>(entity);

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
				if (m_registry.all_of<fr::MaterialComponent>(entity))
				{
					renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<gr::Material::RenderData>>(
						renderDataComponent.GetRenderDataID());
				}

				// Cameras:
				if (m_registry.all_of<fr::CameraComponent>(entity))
				{
					renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<gr::Camera::RenderData>>(
						renderDataComponent.GetRenderDataID());
				}

				// Now the render data components associated with this entity's use of the RenderDataID are destroyed, 
				// we can destroy the render data objects themselves (or decrement the ref. count if it's a shared ID)
				renderManager->EnqueueRenderCommand<gr::DestroyRenderObjectCommand>(
					renderDataComponent.GetRenderDataID());
			}
		}

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
			UpdatePlayerObject(stepTimeMs);
		}

		// ECS_CONVERSION TODO: Clean this up
		
		// Update the scene state:
		UpdateTransforms(); // <- TODO!!! Transforms should be accessed via const ONLY from this point
		UpdateSceneBounds(); // TODO: This is expensive, we should only do this on demand?
		UpdateLightsAndShadows(); // <- DANGER!!! Could potentially modify a (point light) Transform, and its shadow cam's far plane

		UpdateCameras();
	}


	template<typename T, typename RenderDataType>
	void EntityManager::EnqueueRenderUpdateHelper()
	{
		re::RenderManager* renderManager = re::RenderManager::Get();

		auto componentsView = m_registry.view<T, DirtyMarker<T>, gr::RenderDataComponent>();
		for (auto entity : componentsView)
		{
			gr::RenderDataComponent const& renderDataComponent = componentsView.get<gr::RenderDataComponent>(entity);

			T const& component = componentsView.get<T>(entity);

			renderManager->EnqueueRenderCommand<gr::UpdateRenderDataRenderCommand<RenderDataType>>(
				renderDataComponent.GetRenderDataID(),
				T::CreateRenderData(component));

			m_registry.erase<DirtyMarker<T>>(entity);
		}
	}


	void EntityManager::EnqueueRenderUpdates()
	{
		re::RenderManager* renderManager = re::RenderManager::Get();

		// ECS_CONVERSION TODO: Move each of these isolated tasks to a thread

		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

			// Register new render objects:
			auto newRenderableEntitiesView = 
				m_registry.view<NewEntityMarker, gr::RenderDataComponent, gr::RenderDataComponent::NewRegistrationMarker>();
			for (auto entity : newRenderableEntitiesView)
			{
				// Enqueue a command to create a new object on the render thread:
				auto& renderDataComponent = newRenderableEntitiesView.get<gr::RenderDataComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::RegisterRenderObjectCommand>(renderDataComponent);
				
				m_registry.erase<NewEntityMarker>(entity);
				m_registry.erase<gr::RenderDataComponent::NewRegistrationMarker>(entity);
			}

			// Initialize new Transforms. Note: We only send Transforms associated with RenderDataComponents to the 
			// render thread
			auto newTransformComponentsView = 
				m_registry.view<fr::TransformComponent, fr::TransformComponent::NewIDMarker, gr::RenderDataComponent>();
			for (auto entity : newTransformComponentsView)
			{
				fr::TransformComponent& transformComponent =
					newTransformComponentsView.get<fr::TransformComponent>(entity);

				renderManager->EnqueueRenderCommand<fr::UpdateTransformDataRenderCommand>(transformComponent);

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
				}
			}

			// Bounds:
			EnqueueRenderUpdateHelper<fr::BoundsComponent, gr::Bounds::RenderData>();

			// MeshPrimitives:
			EnqueueRenderUpdateHelper<fr::MeshPrimitiveComponent, gr::MeshPrimitive::RenderData>();

			// Materials:
			EnqueueRenderUpdateHelper<fr::MaterialComponent, gr::Material::RenderData>();

			// Cameras:
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

			EnqueueRenderUpdateHelper<fr::CameraComponent, gr::Camera::RenderData>();

			// Update dirty render data components that touch the GraphicsSystems directly:

			//ECS_CONVERSION: MOVE LIGHTS/SHADOWS DIRECTLY TO THE RENDER DATA MANAGER
			// -> Need to have different renderdata types -> Allow GS's to iterate specific light types

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

			// Handle any non-renderable new entities:
			auto newEntitiesView = m_registry.view<NewEntityMarker>();
			for (auto newEntity : newRenderableEntitiesView)
			{
				// For now, we just erase the marker...
				m_registry.erase<NewEntityMarker>(newEntity);
			}
		}
	}


	fr::BoundsComponent const* EntityManager::GetSceneBounds() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			auto sceneBoundsEntityView = m_registry.view<fr::BoundsComponent, fr::BoundsComponent::SceneBoundsMarker>();
			SEAssert("A unique scene bounds entity must exist",
				sceneBoundsEntityView.front() == sceneBoundsEntityView.back());

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


	void EntityManager::SetAsMainCamera(entt::entity camera)
	{
		SEAssert("Entity does not have a valid camera component",
			camera != entt::null && HasComponent<fr::CameraComponent>(camera));

		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			bool foundCurrentMainCamera = false;
			auto currentMainCameraView = m_registry.view<fr::CameraComponent::MainCameraMarker>();
			for (auto entity : currentMainCameraView)
			{
				SEAssert("Already found a main camera. This should not be possible", foundCurrentMainCamera == false);
				foundCurrentMainCamera = true;

				m_registry.erase<fr::CameraComponent::MainCameraMarker>(entity);

				if (m_registry.any_of<fr::CameraComponent::NewMainCameraMarker>(entity))
				{
					m_registry.erase<fr::CameraComponent::NewMainCameraMarker>(entity);
				}
			}

			m_registry.emplace_or_replace<fr::CameraComponent::MainCameraMarker>(camera);
			m_registry.emplace_or_replace<fr::CameraComponent::NewMainCameraMarker>(camera);
		}

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
		EmplaceComponent<NewEntityMarker>(newEntity);

		return newEntity;
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
			SEAssert("Already found a dirty scene bounds. This should not be possible", sceneBoundsDirty == false);
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


	void EntityManager::UpdatePlayerObject(double stepTimeMs)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			fr::CameraComponent* cameraComponent = nullptr;
			fr::TransformComponent* cameraTransform = nullptr;
			bool foundMainCamera = false;
			auto mainCameraView = m_registry.view<
				fr::CameraComponent, fr::CameraComponent::MainCameraMarker, fr::Relationship>();
			for (entt::entity mainCamEntity : mainCameraView)
			{
				SEAssert("Already found a main camera. This should not be possible", foundMainCamera == false);
				foundMainCamera = true;

				fr::Relationship const& cameraRelationship = mainCameraView.get<fr::Relationship>(mainCamEntity);

				cameraComponent = &mainCameraView.get<fr::CameraComponent>(mainCamEntity);

				cameraTransform =
					GetFirstInHierarchyAboveInternal<fr::TransformComponent>(cameraRelationship.GetParent());
			}
			SEAssert("Failed to find main CameraComponent or TransformComponent", cameraComponent && cameraTransform);

			fr::CameraControlComponent* playerObject = nullptr;
			fr::TransformComponent* playerTransform = nullptr;
			bool foundCamController = false;
			auto camControllerView = m_registry.view<fr::CameraControlComponent, fr::TransformComponent>();
			for (entt::entity playerEntity : camControllerView)
			{
				SEAssert("Already found a player object. This should not be possible", foundCamController == false);
				foundCamController = true;

				playerObject = &camControllerView.get<fr::CameraControlComponent>(playerEntity);
				playerTransform = &camControllerView.get<fr::TransformComponent>(playerEntity);
			}
			SEAssert("Failed to find a player object or transform", playerObject && playerTransform);

			fr::CameraControlComponent::Update(
				*playerObject, *playerTransform, *cameraComponent, *cameraTransform, stepTimeMs);
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
				SEAssert("Scene bounds entity already found. This should not be possible", 
					foundSceneBoundsEntity == false);
				foundSceneBoundsEntity = true;

				sceneBoundsEntity = entity;
			}

			// Copy the current bounds so we can detect if it changes
			const fr::BoundsComponent prevBounds = sceneBoundsEntityView.get<fr::BoundsComponent>(sceneBoundsEntity);

			// Modify our bounds component in-place:
			m_registry.patch<fr::BoundsComponent>(sceneBoundsEntity, [&](auto& sceneBoundsComponent)
				{
					auto meshConceptEntitiesView = 
						m_registry.view<fr::Mesh::MeshConceptMarker, fr::BoundsComponent, fr::Relationship>();

					// Make sure we have at least 1 mesh
					auto meshConceptsViewItr = meshConceptEntitiesView.begin();
					if (meshConceptsViewItr != meshConceptEntitiesView.end())
					{
						// Copy the 1st Bounds we view: We'll grow it to encompass all other Bounds
						sceneBoundsComponent = meshConceptEntitiesView.get<fr::BoundsComponent>(*meshConceptsViewItr);
						++meshConceptsViewItr;

						while (meshConceptsViewItr != meshConceptEntitiesView.end())
						{
							const entt::entity meshEntity = *meshConceptsViewItr;

							fr::BoundsComponent const& boundsComponent = 
								meshConceptEntitiesView.get<fr::BoundsComponent>(meshEntity);

							fr::Relationship const& relationshipComponent = 
								meshConceptEntitiesView.get<fr::Relationship>(meshEntity);

							fr::Transform const& meshTransform = GetFirstInHierarchyAboveInternal<fr::TransformComponent>(
								relationshipComponent.GetParent())->GetTransform();

							sceneBoundsComponent.ExpandBounds(
								boundsComponent.GetTransformedAABBBounds(meshTransform.GetGlobalMatrix()));

							++meshConceptsViewItr;
						}
					}
				});

			fr::BoundsComponent const& newSceneBounds = sceneBoundsEntityView.get<fr::BoundsComponent>(sceneBoundsEntity);
			sceneBoundsChanged = (newSceneBounds != prevBounds);
		}

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


	void EntityManager::UpdateLightsAndShadows()
	{
		fr::BoundsComponent const* sceneBounds = GetSceneBounds();

		// Add dirty markers to lights and shadows so the render data will be updated
		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

			// Lights:
			auto lightComponentsView = m_registry.view<fr::LightComponent>();
			for (auto entity : lightComponentsView)
			{
				fr::LightComponent& lightComponent = lightComponentsView.get<fr::LightComponent>(entity);

				// Add a dirty marker to any dirty lights:
				fr::Light& light = lightComponent.GetLight();
				if (light.IsDirty())
				{
					// Deferred point lights: We must update the mesh scale and any shadow camera near/far values
					if (light.GetType() == fr::Light::LightType::Point_Deferred)
					{					
						fr::Relationship const& relationship = m_registry.get<fr::Relationship>(entity);

						fr::TransformComponent& transformCmpt =
							*GetFirstInHierarchyAboveInternal<fr::TransformComponent>(relationship.GetParent());

						fr::Camera* shadowCam = nullptr;
						if (m_registry.any_of<fr::LightComponent::HasShadowMarker>(entity))
						{
							entt::entity shadowMapChild = entt::null;
							fr::ShadowMapComponent* shadowMapCmpt = 
								GetFirstInChildrenInternal<fr::ShadowMapComponent>(entity, shadowMapChild);
							SEAssert("Failed to find shadow map component", shadowMapCmpt);

							entt::entity shadowCamChild = entt::null;
							fr::CameraComponent* shadowCamCmpt = 
								GetFirstInChildrenInternal<fr::CameraComponent>(shadowMapChild, shadowCamChild);
							SEAssert("Failed to find shadow camera", shadowCamCmpt);

							shadowCam = &shadowCamCmpt->GetCameraForModification();
						}

						fr::Light::ConfigurePointLightMeshScale(light, transformCmpt.GetTransform(), shadowCam);
					}

					m_registry.emplace_or_replace<DirtyMarker<fr::LightComponent>>(entity);
					lightComponent.GetLight().MarkClean();
				}
			}

			// Shadows:
			auto shadowMapComponentsView = m_registry.view<fr::ShadowMapComponent>();
			for (auto entity : shadowMapComponentsView)
			{
				const bool force = m_registry.any_of<DirtyMarker<fr::ShadowMapComponent>>(entity);

				fr::TransformComponent const& lightTransformCmpt = 
					*GetFirstInHierarchyAboveInternal<fr::TransformComponent>(entity);

				fr::ShadowMapComponent& shadowMapCmpt = shadowMapComponentsView.get<fr::ShadowMapComponent>(entity);
				
				fr::CameraComponent& shadowCamCmpt = *GetFirstInChildrenInternal<fr::CameraComponent>(entity);

				if (fr::ShadowMapComponent::Update(
					lightTransformCmpt, 
					shadowMapCmpt,
					shadowCamCmpt,
					sceneBounds,
					force))
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
}