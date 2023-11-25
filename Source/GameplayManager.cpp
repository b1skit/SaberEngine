// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Camera.h"
#include "CoreEngine.h"
#include "GameplayManager.h"
#include "PlayerObject.h"
#include "SceneManager.h"

using en::Updateable;
using std::unique_ptr;
using std::shared_ptr;
using std::make_unique;
using std::make_shared;


namespace fr
{
	GameplayManager* GameplayManager::Get()
	{
		static unique_ptr<fr::GameplayManager> instance = make_unique<fr::GameplayManager>();
		return instance.get();
	}


	void GameplayManager::Startup()
	{
		LOG("GameplayManager starting...");

		constexpr size_t k_updateablesReserveAmount = 128; // TODO: Tune this
		m_updateables.reserve(k_updateablesReserveAmount);

		std::shared_ptr<gr::Camera> mainCam = en::SceneManager::Get()->GetMainCamera();

		// Add a player object to the scene:
		m_playerObject = std::make_shared<fr::PlayerObject>(mainCam);

		LOG("Created PlayerObject using \"%s\"", mainCam->GetName().c_str());
	}


	void GameplayManager::Shutdown()
	{
		LOG("GameplayManager shutting down...");

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

		m_registry.clear();
	}


	void GameplayManager::Update(uint64_t frameNum, double stepTimeMs)
	{
		UpdateUpdateables(stepTimeMs);
		UpdateTransformables();
	}


	entt::entity GameplayManager::CreateEntity()
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);
			return m_registry.create();
		}
	}


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