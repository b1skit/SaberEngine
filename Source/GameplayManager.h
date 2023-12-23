// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
#include "EngineComponent.h"
#include "Updateable.h"
#include "BoundsComponent.h"


namespace fr
{
	class PlayerObject;


	class GameplayManager final : public virtual en::EngineComponent
	{
	public:
		static GameplayManager* Get(); // Singleton functionality


	public:
		void Startup() override;
		void Shutdown() override;

		void Update(uint64_t frameNum, double stepTimeMs) override;

		void EnqueueRenderUpdates();

		fr::BoundsComponent const& GetSceneBounds() const;


		// EnTT wrappers:
	public:
		entt::entity CreateEntity(std::string const& name);
		entt::entity CreateEntity(char const* name);

		template<typename T>
		entt::entity GetEntityFromComponent(T const&) const;

		template<typename T>
		void EmplaceComponent(entt::entity);

		template<typename T, typename... Args>
		T* EmplaceComponent(entt::entity, Args &&...args);

		template<typename T>
		void EmplaceOrReplaceComponent(entt::entity);

		template<typename T>
		void TryEmplaceComponent(entt::entity); // Emplace a component IFF it doesn't already exist on the entity

		template<typename T, typename... Args>
		T* TryEmplaceComponent(entt::entity, Args &&...args);

		template<typename T, typename... Args>
		T* GetOrEmplaceComponent(entt::entity, Args &&...args);

		template<typename T>
		void RemoveComponent(entt::entity);

		template<typename T>
		T& GetComponent(entt::entity);

		template<typename T>
		T const& GetComponent(entt::entity) const;

		template<typename T>
		T* TryGetComponent(entt::entity entity);

		template<typename T>
		T const* TryGetComponent(entt::entity entity) const;

		template<typename T>
		bool HasComponent(entt::entity entity) const;

		template<typename T, typename... Args>
		auto CreateView();


	private:
		entt::basic_registry<entt::entity> m_registry; // uint32_t entities
		mutable std::shared_mutex m_registeryMutex;


	private: // Systems:
	public:
		void UpdateSceneBounds();
		// ECS_CONVERSION TODO: Make this private again. Currently public as a hack so we can trigger it after scene loading
	private:
		void UpdateTransformComponents();
		void UpdateLights();


		// DEPRECATED:
	protected:
		friend class en::Updateable;
		void AddUpdateable(en::Updateable* updateable);
		void RemoveUpdateable(en::Updateable const* updateable);


	private:
		void UpdateUpdateables(double stepTimeMs) const;


	private:
		std::vector<en::Updateable*> m_updateables;
		mutable std::mutex m_updateablesMutex;

		std::shared_ptr<fr::PlayerObject> m_playerObject;
	};


	template<typename T>
	entt::entity GameplayManager::GetEntityFromComponent(T const& component) const
	{
		entt::entity entity = entt::null;
		{
			std::shared_lock<std::shared_mutex> lock(m_registeryMutex);

			entity = entt::to_entity(m_registry, component);
		}
		SEAssert("Entity not found", entity != entt::null);

		return entity;
	}


	template<typename T>
	void GameplayManager::EmplaceComponent(entt::entity entity)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			m_registry.emplace<T>(entity);
		}
	}


	template<typename T, typename... Args>
	T* GameplayManager::EmplaceComponent(entt::entity entity, Args&&... args)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			T& result = m_registry.emplace<T>(entity, std::forward<Args>(args)...);

			return &result;
		}
	}


	template<typename T>
	void GameplayManager::EmplaceOrReplaceComponent(entt::entity entity)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			m_registry.emplace_or_replace<T>(entity);
		}
	}


	template<typename T>
	void GameplayManager::TryEmplaceComponent(entt::entity entity)
	{
		T* existingComponent = TryGetComponent<T>(entity);
		if (existingComponent == nullptr)
		{
			EmplaceComponent<T>(entity);
		}
	}


	template<typename T, typename... Args>
	T* GameplayManager::TryEmplaceComponent(entt::entity entity, Args &&...args)
	{
		T* existingComponent = TryGetComponent<T>(entity);
		if (existingComponent == nullptr)
		{
			existingComponent = EmplaceComponent<T>(entity, std::forward<Args>(args)...);
		}
		return existingComponent;
	}


	template<typename T, typename... Args>
	T* GameplayManager::GetOrEmplaceComponent(entt::entity entity, Args&&... args)
	{
		T* existingComponent = TryGetComponent<T>(entity);
		if (existingComponent == nullptr)
		{
			existingComponent = EmplaceComponent<T, Args...>(entity, std::forward<Args>(args)...);
		}
		return existingComponent;
	}


	template<typename T>
	void GameplayManager::RemoveComponent(entt::entity entity)
	{
		{
			// It's only safe to add/remove/iterate components if no other thread is adding/removing/iterating
			// components of the same type. For now, we obtain an exclusive lock on the entire registry, but this could
			// be more granular
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			m_registry.erase<T>(entity);
		}
	}


	template<typename T>
	T* GameplayManager::TryGetComponent(entt::entity entity)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			return m_registry.try_get<T>(entity);
		}
	}


	template<typename T>
	T const* GameplayManager::TryGetComponent(entt::entity entity) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			return m_registry.try_get<T>(entity);
		}
	}


	template<typename T>
	bool GameplayManager::HasComponent(entt::entity entity) const
	{
		return TryGetComponent<T>(entity) != nullptr;
	}


	template<typename T>
	T& GameplayManager::GetComponent(entt::entity entity)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);
			return m_registry.get<T>(entity);
		}
	}


	template<typename T>
	T const& GameplayManager::GetComponent(entt::entity entity) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);
			return m_registry.get<T>(entity);
		}
	}


	template<typename T, typename... Args>
	auto GameplayManager::CreateView()
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);
			return m_registry.view<T, Args...>();
		}
	}
}