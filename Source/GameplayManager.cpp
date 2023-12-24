// © 2022 Adam Badke. All rights reserved.
#include "CameraComponent.h"
#include "CoreEngine.h"
#include "GameplayManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "MaterialComponent.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "PlayerObject.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"
#include "ShadowMapComponent.h"


namespace fr
{
	GameplayManager* GameplayManager::Get()
	{
		static std::unique_ptr<fr::GameplayManager> instance = std::make_unique<fr::GameplayManager>();
		return instance.get();
	}


	void GameplayManager::Startup()
	{
		LOG("GameplayManager starting...");

		constexpr size_t k_updateablesReserveAmount = 128; // TODO: Tune this
		m_updateables.reserve(k_updateablesReserveAmount);

		// Create a scene bounds entity:
		fr::BoundsComponent::CreateSceneBoundsConcept(*this);
		UpdateSceneBounds();

		// Create an Ambient light:
		fr::LightComponent::CreateDeferredAmbientLightConcept(en::SceneManager::GetSceneData()->GetIBLTexture());

		std::shared_ptr<fr::Camera> mainCam = en::SceneManager::Get()->GetMainCamera();

		// Add a player object to the scene:
		m_playerObject = std::make_shared<fr::PlayerObject>(mainCam);

		LOG("Created PlayerObject using \"%s\"", mainCam->GetName().c_str());

		// Push render updates to ensure new data is available for the first frame
		EnqueueRenderUpdates();
	}


	void GameplayManager::Shutdown()
	{
		LOG("GameplayManager shutting down...");

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
				if (m_registry.all_of<fr::MeshPrimitive::MeshPrimitiveComponent>(entity))
				{
					renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<gr::MeshPrimitive::RenderData>>(
						renderDataComponent.GetRenderDataID());
				}


				// Materials:
				if (m_registry.all_of<fr::MeshPrimitive::MeshPrimitiveComponent>(entity))
				{
					renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<gr::Material::RenderData>>(
						renderDataComponent.GetRenderDataID());
				}
			}

			// Now the render data components are destroyed, we can destroy the render data objects themselves:
			for (auto entity : renderDataEntitiesView)
			{
				auto& renderDataComponent = renderDataEntitiesView.get<gr::RenderDataComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::DestroyRenderObjectCommand>(
					renderDataComponent.GetRenderDataID());
			}
		}

		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);
			m_registry.clear();
		}


		// ECS_CONVERSION TODO: Call GameplayManager::Shutdown() before shutting down the renderer
		// -> Once the Updateables/Transformables have been converted to ECS...
		


		// DEPRECATED: 
		m_playerObject = nullptr;

		// Destroy self-registered interfaces
		{
			std::lock_guard<std::mutex> lock(m_updateablesMutex);

			SEAssert("Updateables should have been destroyed and self-unregistered by now", m_updateables.empty());
			m_updateables.clear();
		}
	}


	void GameplayManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// Handle input (update event listener components)
		// <physics, animation, etc>

		// Update the scene state:
		UpdateTransforms();
		UpdateSceneBounds(); // TODO: This is expensive, we should only do this on demand?
		UpdateLights(); // <- Could potentially modify a (point light) Transform, and its shadow cam's far plane
		
		// ECS_CONVERSION: BEWARE: Cameras have a transform and may modify it (i.e by calling GetGlobal___() ).
		// -> Need a way of providing a const Transform and const Transform::Get___() functions
		//		-> Guarantee Transforms have been updated already and are clean, and then assert in the const functions
		//			that this is always true
		UpdateCameras(); 




		// Deprecated:
		UpdateUpdateables(stepTimeMs);
	}


	void GameplayManager::EnqueueRenderUpdates()
	{
		re::RenderManager* renderManager = re::RenderManager::Get();

		// ECS_CONVERSION TODO: Move each of these isolated tasks to a thread

		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

			// Register new render objects:
			auto newRenderableEntitiesView = 
				m_registry.view<NewEntityMarker, gr::RenderDataComponent, gr::RenderDataComponent::NewIDMarker>();
			for (auto entity : newRenderableEntitiesView)
			{
				// Enqueue a command to create a new object on the render thread:
				auto& renderDataComponent = newRenderableEntitiesView.get<gr::RenderDataComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::RegisterRenderObjectCommand>(renderDataComponent);
				
				m_registry.erase<NewEntityMarker>(entity);
				m_registry.erase<gr::RenderDataComponent::NewIDMarker>(entity);
			}

			// Initialize new Transforms:
			auto newTransformComponentsView = 
				m_registry.view<fr::TransformComponent, fr::TransformComponent::NewIDMarker>();
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
			auto transformComponentsView = m_registry.view<fr::TransformComponent>();
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
			// Bounds are stored in local space & transformed as needed. We just need to copy them to the render thread
			auto boundsComponentsView = m_registry.view<fr::BoundsComponent, DirtyMarker<fr::BoundsComponent>, gr::RenderDataComponent>();
			for (auto entity : boundsComponentsView)
			{
				gr::RenderDataComponent const& renderDataComponent = 
					boundsComponentsView.get<gr::RenderDataComponent>(entity);

				fr::BoundsComponent const& boundsComponent = boundsComponentsView.get<fr::BoundsComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::UpdateRenderDataRenderCommand<gr::Bounds::RenderData>>(
					renderDataComponent.GetRenderDataID(),
					fr::BoundsComponent::CreateRenderData(boundsComponent));

				m_registry.erase<DirtyMarker<fr::BoundsComponent>>(entity);
			}

			// MeshPrimitives:
			// The actual data of a MeshPrimitive is SceneData; We push pointers and metadata to the render thread
			auto meshPrimitiveComponentsView = m_registry.view<
				fr::MeshPrimitive::MeshPrimitiveComponent, 
				DirtyMarker<fr::MeshPrimitive::MeshPrimitiveComponent>, 
				gr::RenderDataComponent>();
			for (auto entity : meshPrimitiveComponentsView)
			{
				gr::RenderDataComponent const& renderDataComponent =
					meshPrimitiveComponentsView.get<gr::RenderDataComponent>(entity);

				fr::MeshPrimitive::MeshPrimitiveComponent const& meshPrimComponent = 
					meshPrimitiveComponentsView.get<fr::MeshPrimitive::MeshPrimitiveComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::UpdateRenderDataRenderCommand<gr::MeshPrimitive::RenderData>>(
					renderDataComponent.GetRenderDataID(), fr::MeshPrimitive::CreateRenderData(meshPrimComponent));

				m_registry.erase<DirtyMarker<fr::MeshPrimitive::MeshPrimitiveComponent>>(entity);
			}

			// Materials:
			// Material data is (currently) SceneData; We push pointers to the render thread. TODO: Allow Material
			// instancing and dynamic modification
			auto materialComponentsView = m_registry.view<
				fr::Material::MaterialComponent, 
				DirtyMarker<fr::Material::MaterialComponent>, 
				gr::RenderDataComponent>();
			for (auto entity : materialComponentsView)
			{
				gr::RenderDataComponent const& renderDataComponent =
					materialComponentsView.get<gr::RenderDataComponent>(entity);

				fr::Material::MaterialComponent const& materialComponent =
					materialComponentsView.get<fr::Material::MaterialComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::UpdateRenderDataRenderCommand<gr::Material::RenderData>>(
					renderDataComponent.GetRenderDataID(), fr::Material::CreateRenderData(materialComponent));

				m_registry.erase<DirtyMarker<fr::Material::MaterialComponent>>(entity);
			}

			// Cameras:
			auto cameraComponentsView =
				m_registry.view<fr::CameraComponent, DirtyMarker<fr::CameraComponent>, gr::RenderDataComponent>();
			for (auto entity : cameraComponentsView)
			{
				gr::RenderDataComponent const& renderDataComponent =
					cameraComponentsView.get<gr::RenderDataComponent>(entity);

				fr::CameraComponent& cameraCmpt = cameraComponentsView.get<fr::CameraComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::UpdateRenderDataRenderCommand<gr::Camera::RenderData>>(
					renderDataComponent.GetRenderDataID(), fr::CameraComponent::CreateRenderData(cameraCmpt));

				m_registry.erase<DirtyMarker<fr::CameraComponent>>(entity);
			}

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

			// Handle any non-renderable new entities:
			auto newEntitiesView = m_registry.view<NewEntityMarker>();
			for (auto newEntity : newRenderableEntitiesView)
			{
				// For now, we just erase the marker...
				m_registry.erase<NewEntityMarker>(newEntity);
			}
		}
	}


	fr::BoundsComponent const* GameplayManager::GetSceneBounds() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			auto sceneBoundsEntityView = m_registry.view<fr::BoundsComponent, fr::BoundsComponent::IsSceneBoundsMarker>();
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


	entt::entity GameplayManager::CreateEntity(std::string const& name)
	{
		return CreateEntity(name.c_str());
	}


	entt::entity GameplayManager::CreateEntity(char const* name)
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


	void GameplayManager::UpdateSceneBounds()
	{
		// ECS_CONVERSION TODO: This should be triggered by listening for when MESH bounds are added/updated
		
		entt::entity sceneBoundsEntity = entt::null;
		bool sceneBoundsChanged = false;
		{
			// We're only viewing the registry and modifying components in place; Only need a read lock
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			auto sceneBoundsEntityView = m_registry.view<fr::BoundsComponent, fr::BoundsComponent::IsSceneBoundsMarker>();
			SEAssert("A unique scene bounds entity must exist",
				sceneBoundsEntityView.front() == sceneBoundsEntityView.back());
			
			sceneBoundsEntity = sceneBoundsEntityView.front();

			// Copy the current bounds so we can detect if it changes
			const fr::BoundsComponent prevBounds = sceneBoundsEntityView.get<fr::BoundsComponent>(sceneBoundsEntity);

			// Modify our bounds component in-place:
			m_registry.patch<fr::BoundsComponent>(sceneBoundsEntity, [&](auto& sceneBoundsComponent)
				{
					auto meshConceptEntitiesView = m_registry.view<fr::Mesh::MeshConceptMarker>();

					// Make sure we have at least 1 mesh
					auto meshConceptsViewItr = meshConceptEntitiesView.begin();
					if (meshConceptsViewItr != meshConceptEntitiesView.end())
					{
						// Reset our bounds to be the 1st Bounds we view: It'll grow to encompass all other Bounds
						sceneBoundsComponent = m_registry.get<fr::BoundsComponent>(*meshConceptsViewItr);
						++meshConceptsViewItr;

						while (meshConceptsViewItr != meshConceptEntitiesView.end())
						{
							const entt::entity meshEntity = *meshConceptsViewItr;
							fr::BoundsComponent const& boundsComponent = m_registry.get<fr::BoundsComponent>(meshEntity);

							fr::Relationship const& relationshipComponent = m_registry.get<fr::Relationship>(meshEntity);

							fr::Transform& meshTransform = GetFirstInHierarchyAboveInternal<fr::TransformComponent>(
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



			// ECS_CONVERSION TEMP HAX!!!!!!!!!!!!!!!!!!!
			// -> TODO: Write a system to listen for a dirty marker being placed, and trigger this automatically
			auto directionalLightShadowsView = 
				m_registry.view<fr::ShadowMapComponent, fr::LightComponent::DirectionalDeferredMarker>();
			for (auto entity : directionalLightShadowsView)
			{
				fr::ShadowMapComponent::MarkDirty(*this, entity);
			}
		}
	}


	void GameplayManager::UpdateTransforms()
	{
		// Use the number of root transforms during the last update 
		static size_t prevNumRootTransforms = 1;

		std::vector<std::future<void>> taskFutures;
		taskFutures.reserve(prevNumRootTransforms);

		// ECS_CONVERSION: Add a read lock here!!!!

		auto transformComponentsView = m_registry.view<fr::TransformComponent>();
		for (auto entity : transformComponentsView)
		{
			fr::TransformComponent& transformComponent =
				transformComponentsView.get<fr::TransformComponent>(entity);

			// Find root nodes:
			fr::Transform& node = transformComponent.GetTransform();
			if (node.GetParent() == nullptr)
			{
				fr::TransformComponent::DispatchTransformUpdateThread(taskFutures, &node);
			}
		}

		prevNumRootTransforms = std::max(1llu, taskFutures.size());

		// Wait for the updates to complete
		for (std::future<void> const& taskFuture : taskFutures)
		{
			taskFuture.wait();
		}
	}


	void GameplayManager::UpdateLights()
	{
		// Add dirty markers to lights and shadows so the render data will be updated
		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

			auto lightComponentsView = m_registry.view<fr::LightComponent>();
			for (auto entity : lightComponentsView)
			{
				fr::LightComponent& lightComponent = lightComponentsView.get<fr::LightComponent>(entity);

				fr::Light& light = lightComponent.GetLight();
				if (light.IsDirty())
				{
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

							shadowCam = &shadowCamCmpt->GetCamera();
						}

						fr::Light::ConfigurePointLightMeshScale(light, transformCmpt.GetTransform(), shadowCam);
					}

					m_registry.emplace_or_replace<DirtyMarker<fr::LightComponent>>(entity);
					lightComponent.GetLight().MarkClean();
				}
			}

			auto shadowMapComponentsView = m_registry.view<fr::ShadowMapComponent>();
			for (auto entity : shadowMapComponentsView)
			{
				fr::ShadowMapComponent& shadowMapComponent = shadowMapComponentsView.get<fr::ShadowMapComponent>(entity);

				if (shadowMapComponent.GetShadowMap().IsDirty())
				{
					m_registry.emplace_or_replace<DirtyMarker<fr::ShadowMapComponent>>(entity);
					shadowMapComponent.GetShadowMap().MarkClean();
				}
			}
		}
	}


	void GameplayManager::UpdateCameras()
	{
		// Check for dirty cameras, or cameras with dirty transforms
		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

			auto cameraComponentsView = m_registry.view<fr::CameraComponent, fr::Relationship>();
			for (auto entity : cameraComponentsView)
			{
				fr::CameraComponent& cameraComponent = cameraComponentsView.get<fr::CameraComponent>(entity);

				fr::Camera& camera = cameraComponent.GetCamera();
				if (camera.IsDirty() || camera.GetTransform()->HasChanged())
				{
					m_registry.emplace_or_replace<DirtyMarker<fr::CameraComponent>>(entity);
					camera.MarkClean();
				}
			}
		}
	}


	// DEPRECATED:

	void GameplayManager::UpdateUpdateables(double stepTimeMs) const
	{
		{
			std::lock_guard<std::mutex> lock(m_updateablesMutex);

			for (en::Updateable* updateable : m_updateables)
			{
				updateable->Update(stepTimeMs);
			}
		}
	}


	void GameplayManager::AddUpdateable(en::Updateable* updateable)
	{
		SEAssert("Updateable cannot be null ", updateable);

		{
			std::lock_guard<std::mutex> lock(m_updateablesMutex);
			m_updateables.emplace_back(updateable);
		}
	}


	void GameplayManager::RemoveUpdateable(en::Updateable const* updateable)
	{
		SEAssert("Updateable cannot be null ", updateable);

		{
			std::lock_guard<std::mutex> lock(m_updateablesMutex);

			auto const& result = std::find_if(
				m_updateables.begin(),
				m_updateables.end(),
				[&](en::Updateable const* curUpdateable) { return curUpdateable == updateable; });

			SEAssert("Updateable not found", result != m_updateables.end());

			m_updateables.erase(result);
		}
	}
}