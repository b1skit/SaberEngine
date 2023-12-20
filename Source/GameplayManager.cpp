// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "CoreEngine.h"
#include "GameplayManager.h"
#include "MarkerComponents.h"
#include "Material_GLTF.h"
#include "NameComponent.h"
#include "PlayerObject.h"
#include "Relationship.h"
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"


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
		/*fr::Bounds::CreateSceneBounds(*this);*/
		UpdateSceneBounds();

		// ECS_CONVERSION: Currently, the GameplayManager::Startup is called after the SceneManager and RenderManager,
		// as it needs the main camera from the SceneData to create the player obect
		// -> This is a problem, as we need the scene bounds to be available to initialize shadow maps immediately after
		// the scene has loaded. For now, we hack around this
		
		std::shared_ptr<gr::Camera> mainCam = en::SceneManager::Get()->GetMainCamera();

		// Add a player object to the scene:
		m_playerObject = std::make_shared<fr::PlayerObject>(mainCam);

		LOG("Created PlayerObject using \"%s\"", mainCam->GetName().c_str());
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
				if (m_registry.all_of<fr::Bounds>(entity))
				{
					renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<fr::Bounds::RenderData>>(
						renderDataComponent.GetRenderObjectID());
				}

				// MeshPrimitives:
				if (m_registry.all_of<gr::MeshPrimitive::MeshPrimitiveComponent>(entity))
				{
					renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<gr::MeshPrimitive::RenderData>>(
						renderDataComponent.GetRenderObjectID());
				}


				// Materials:
				if (m_registry.all_of<gr::MeshPrimitive::MeshPrimitiveComponent>(entity))
				{
					renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<gr::Material::RenderData>>(
						renderDataComponent.GetRenderObjectID());
				}
			}

			// Now the render data components are destroyed, we can destroy the render data objects themselves:
			for (auto entity : renderDataEntitiesView)
			{
				auto& renderDataComponent = renderDataEntitiesView.get<gr::RenderDataComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::DestroyRenderObjectCommand>(
					renderDataComponent.GetRenderObjectID());
			}
		}

		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);
			m_registry.clear();
		}


		// TODO: Call GameplayManager::Shutdown() before shutting down the renderer
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
		UpdateTransformComponents();
		UpdateSceneBounds(); // TODO: This is expensive, we should only do this on demand?
		// Update Lights (Need bounds, transforms to be updated)
		// Update Cameras
		// Update Renderables




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
			auto boundsComponentsView = m_registry.view<fr::Bounds, DirtyMarker<fr::Bounds>, gr::RenderDataComponent>();
			for (auto entity : boundsComponentsView)
			{
				gr::RenderDataComponent const& renderDataComponent = 
					boundsComponentsView.get<gr::RenderDataComponent>(entity);

				fr::Bounds const& boundsComponent = boundsComponentsView.get<fr::Bounds>(entity);

				renderManager->EnqueueRenderCommand<gr::UpdateRenderDataRenderCommand<fr::Bounds::RenderData>>(
					renderDataComponent.GetRenderObjectID(),
					fr::Bounds::GetRenderData(boundsComponent));

				m_registry.erase<DirtyMarker<fr::Bounds>>(entity);
			}

			// MeshPrimitives:
			// The actual data of a MeshPrimitive is SceneData; We push pointers and metadata to the render thread
			auto meshPrimitiveComponentsView = m_registry.view<
				gr::MeshPrimitive::MeshPrimitiveComponent, 
				DirtyMarker<gr::MeshPrimitive::MeshPrimitiveComponent>, 
				gr::RenderDataComponent>();
			for (auto entity : meshPrimitiveComponentsView)
			{
				gr::RenderDataComponent const& renderDataComponent =
					meshPrimitiveComponentsView.get<gr::RenderDataComponent>(entity);

				gr::MeshPrimitive::MeshPrimitiveComponent const& meshPrimComponent = 
					meshPrimitiveComponentsView.get<gr::MeshPrimitive::MeshPrimitiveComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::UpdateRenderDataRenderCommand<gr::MeshPrimitive::RenderData>>(
					renderDataComponent.GetRenderObjectID(), gr::MeshPrimitive::GetRenderData(meshPrimComponent));

				m_registry.erase<DirtyMarker<gr::MeshPrimitive::MeshPrimitiveComponent>>(entity);
			}

			// Materials:
			// Material data is (currently) SceneData; We push pointers to the render thread. TODO: Allow Material
			// instancing and dynamic modification
			auto materialComponentsView = m_registry.view<
				gr::Material::MaterialComponent, 
				DirtyMarker<gr::Material::MaterialComponent>, 
				gr::RenderDataComponent>();
			for (auto entity : materialComponentsView)
			{
				gr::RenderDataComponent const& renderDataComponent =
					materialComponentsView.get<gr::RenderDataComponent>(entity);

				gr::Material::MaterialComponent const& materialComponent =
					materialComponentsView.get<gr::Material::MaterialComponent>(entity);

				renderManager->EnqueueRenderCommand<gr::UpdateRenderDataRenderCommand<gr::Material::RenderData>>(
					renderDataComponent.GetRenderObjectID(), gr::Material::GetRenderData(materialComponent));

				m_registry.erase<DirtyMarker<gr::Material::MaterialComponent>>(entity);
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


	fr::Bounds const& GameplayManager::GetSceneBounds() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			auto sceneBoundsEntityView = m_registry.view<fr::Bounds, fr::Bounds::IsSceneBoundsMarker>();
			SEAssert("A unique scene bounds entity must exist",
				sceneBoundsEntityView.front() == sceneBoundsEntityView.back());

			return sceneBoundsEntityView.get<fr::Bounds>(sceneBoundsEntityView.front());
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

		EmplaceComponent<NameComponent>(newEntity, name);
		EmplaceComponent<NewEntityMarker>(newEntity);

		return newEntity;
	}


	void GameplayManager::UpdateSceneBounds()
	{
		// ECS_CONVERSION TODO: This is a nasty hack to ensure the scene bounds is valid when it needs to be, until the
		// startup order gets straightened out
		static bool s_hasCreatedSceneBounds_HACK = false;
		if (!s_hasCreatedSceneBounds_HACK)
		{
			fr::Bounds::CreateSceneBounds(*this);
			s_hasCreatedSceneBounds_HACK = true;
		}

		// ECS_CONVERSION TODO: This should be triggered by listening for when bounds are added/updated
		
		entt::entity sceneBoundsEntity = entt::null;
		bool sceneBoundsChanged = false;
		{
			// We're only viewing the registry and modifying components in place; Only need a read lock
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			auto sceneBoundsEntityView = m_registry.view<fr::Bounds, fr::Bounds::IsSceneBoundsMarker>();
			SEAssert("A unique scene bounds entity must exist",
				sceneBoundsEntityView.front() == sceneBoundsEntityView.back());
			
			sceneBoundsEntity = sceneBoundsEntityView.front();

			// Copy the current bounds so we can detect if it changes
			const fr::Bounds prevBounds = sceneBoundsEntityView.get<fr::Bounds>(sceneBoundsEntity);

			// Modify our bounds component in-place:
			m_registry.patch<fr::Bounds>(sceneBoundsEntity, [&](auto& sceneBoundsComponent)
				{
					// Reset our bounds: It'll grow to encompass all bounds
					sceneBoundsComponent = Bounds();

					auto meshConceptsView = m_registry.view<fr::Mesh::MeshConceptMarker>();					
					for (auto const& meshEntity : meshConceptsView)
					{
						fr::Bounds const& boundsComponent = m_registry.get<fr::Bounds>(meshEntity);

						fr::Relationship const& relationshipComponent = m_registry.get<fr::Relationship>(meshEntity);

						gr::Transform& sceneNodeTransform = 
							fr::SceneNode::GetSceneNodeTransform(relationshipComponent.GetParent());

						sceneBoundsComponent.ExpandBounds(
							boundsComponent.GetTransformedAABBBounds(sceneNodeTransform.GetGlobalMatrix()));
					}
				});

			fr::Bounds const& newSceneBounds = sceneBoundsEntityView.get<fr::Bounds>(sceneBoundsEntity);
			sceneBoundsChanged = (newSceneBounds != prevBounds);
		}

		if (sceneBoundsChanged)
		{
			EmplaceOrReplaceComponent<DirtyMarker<fr::Bounds>>(sceneBoundsEntity);
		}
	}


	void GameplayManager::UpdateTransformComponents()
	{
		// Use the number of root transforms during the last update 
		static size_t prevNumRootTransforms = 1;

		std::vector<std::future<void>> taskFutures;
		taskFutures.reserve(prevNumRootTransforms);

		auto transformComponentsView = m_registry.view<fr::TransformComponent>();
		for (auto entity : transformComponentsView)
		{
			fr::TransformComponent& transformComponent =
				transformComponentsView.get<fr::TransformComponent>(entity);

			// Find root nodes:
			gr::Transform& node = transformComponent.GetTransform();
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
				[&](en::Updateable const* curUpdateable) {return curUpdateable == updateable; });

			SEAssert("Updateable not found", result != m_updateables.end());

			m_updateables.erase(result);
		}
	}
}