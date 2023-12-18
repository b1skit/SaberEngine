// � 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "CoreEngine.h"
#include "GameplayManager.h"
#include "MarkerComponents.h"
#include "NameComponent.h"
#include "PlayerObject.h"
#include "Relationship.h"
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "SceneManager.h"


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
		fr::Bounds::CreateSceneBounds(*this);
		UpdateSceneBounds();

		
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

			auto renderDataEntitiesView = m_registry.view<gr::RenderDataComponent>();
			for (auto renderable : renderDataEntitiesView)
			{
				auto& renderDataComponent = renderDataEntitiesView.get<gr::RenderDataComponent>(renderable);

				// Destroy render data components:
				if (m_registry.all_of<fr::Bounds>(renderable))
				{

					renderManager->EnqueueRenderCommand<gr::DestroyRenderDataRenderCommand<fr::Bounds>>(
						renderDataComponent.GetRenderObjectIDs()[0]);
				}

				// Finally, destroy the render objects:
				for (auto& renderObjectID : renderDataComponent.GetRenderObjectIDs())
				{
					renderManager->EnqueueRenderCommand<gr::DestroyRenderObjectCommand>(renderObjectID);
				}				
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
		{
			std::lock_guard<std::mutex> lock(m_transformablesMutex);

			SEAssert("Transformables should have been destroyed and self-unregistered by now", m_transformables.empty());

			m_transformables.clear();
		}
	}


	void GameplayManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		// Handle input (update event listener components)
		// <physics, animation, etc>
		// Update Transforms
		UpdateSceneBounds(); // TODO: This is expensive, we should only do this on demand?
		// Update Lights (Need bounds, transforms to be updated)
		// Update Cameras
		// Update Renderables




		// Deprecated:
		UpdateUpdateables(stepTimeMs);
		UpdateTransformables();
	}


	void GameplayManager::EnqueueRenderUpdates()
	{
		re::RenderManager* renderManager = re::RenderManager::Get();

		{
			std::unique_lock<std::shared_mutex> writeLock(m_registeryMutex);

			// Register new render objects:
			auto newRenderableEntitiesView = m_registry.view<NewEntityMarker, gr::RenderDataComponent>();
			for (auto newEntity : newRenderableEntitiesView)
			{
				// Enqueue a command to create a new object on the render thread:
				auto& renderDataComponent = newRenderableEntitiesView.get<gr::RenderDataComponent>(newEntity);
				renderManager->EnqueueRenderCommand<gr::CreateRenderObjectCommand>(
					renderDataComponent.GetRenderObjectIDs()[0]);

				m_registry.erase<NewEntityMarker>(newEntity);
			}

			// Update dirty component data:
			auto renderDataComponentView = m_registry.view<gr::RenderDataComponent>();
			for (auto renderableEntity : renderDataComponentView)
			{
				auto& renderDataComponent = newRenderableEntitiesView.get<gr::RenderDataComponent>(renderableEntity);

				// BoundsConcept:
				if (m_registry.all_of<fr::Bounds, DirtyMarker<fr::Bounds>>(renderableEntity))
				{
					renderManager->EnqueueRenderCommand<gr::UpdateRenderDataRenderCommand<fr::Bounds::RenderData>>(
						renderDataComponent.GetRenderObjectIDs()[0],
						fr::Bounds::CreateRenderData(m_registry.get<fr::Bounds>(renderableEntity)));

					m_registry.erase<DirtyMarker<fr::Bounds>>(renderableEntity);
				}
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
		{
			// We're only viewing the registry and modifying components in place; Only need a read lock
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			auto sceneBoundsEntityView = m_registry.view<fr::Bounds, fr::Bounds::IsSceneBoundsMarker>();
			SEAssert("A unique scene bounds entity must exist",
				sceneBoundsEntityView.front() == sceneBoundsEntityView.back());

			// Copy the current bounds so we can detect if it changes
			const fr::Bounds prevBounds = sceneBoundsEntityView.get<fr::Bounds>(sceneBoundsEntityView.front());

			// Modify our bounds component in-place:
			m_registry.patch<fr::Bounds>(sceneBoundsEntityView.front(), [&](auto& boundsComponent)
				{
					// Reset our bounds: It'll grow to encompass all bounds
					boundsComponent = Bounds();

					// Check each mesh:
					auto meshEntities = m_registry.view<gr::Mesh>();
					for (auto entity : meshEntities)
					{
						auto& meshComponent = m_registry.get<gr::Mesh>(entity);

						boundsComponent.ExpandBounds(meshComponent.GetBounds().GetTransformedAABBBounds(
							meshComponent.GetTransform()->GetGlobalMatrix()));
					}
				});

			if (sceneBoundsEntityView.get<fr::Bounds>(sceneBoundsEntityView.front()) != prevBounds)
			{
				EmplaceComponent<DirtyMarker<fr::Bounds>>(sceneBoundsEntityView.front());
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


	void GameplayManager::UpdateTransformables() const
	{
		{
			std::lock_guard<std::mutex> lock(m_transformablesMutex);
			// TODO: It's still hypothetically possible that another thread could modify a transform while this process
			// happens. It could be worth locking all transforms in the entire hierarchy here first

			std::vector<std::future<void>> taskFutures;
			taskFutures.reserve(m_transformables.size());
			for (fr::Transformable* transformable : m_transformables)
			{
				// We're interested in root nodes only. We could cache these to avoid the search, but for now it's fine
				gr::Transform* currentTransform = transformable->GetTransform();
				if (currentTransform->GetParent() == nullptr)
				{
					// DFS walk down our Transform hierarchy, recomputing each Transform in turn. The goal here is to
					// minimize the (re)computation required when we copy Transforms for the Render thread
					std::stack<gr::Transform*> transforms;
					transforms.push(currentTransform);

					taskFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
						[currentTransform]()
						{
							std::stack<gr::Transform*> transforms;
							transforms.push(currentTransform);

							while (!transforms.empty())
							{
								gr::Transform* topTransform = transforms.top();
								transforms.pop();

								topTransform->ClearHasChangedFlag();
								topTransform->Recompute();

								for (gr::Transform* child : topTransform->GetChildren())
								{
									transforms.push(child);
								}
							}
						}));
				}
			}
			for (std::future<void> const& taskFuture : taskFutures)
			{
				taskFuture.wait();
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


	void GameplayManager::AddTransformable(fr::Transformable* transformable)
	{
		{
			std::lock_guard<std::mutex> lock(m_transformablesMutex);
			m_transformables.emplace_back(transformable);
		}
	}


	void GameplayManager::RemoveTransformable(fr::Transformable const* transformable)
	{
		{
			std::lock_guard<std::mutex> lock(m_transformablesMutex);

			auto const& result = std::find_if(
				m_transformables.begin(),
				m_transformables.end(),
				[&](fr::Transformable* curTransformable) {return curTransformable == transformable; });

			SEAssert("Transformable not found", result != m_transformables.end());

			if (result != m_transformables.end())
			{
				m_transformables.erase(result);
			}
		}
	}
}