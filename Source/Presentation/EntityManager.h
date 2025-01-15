// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "EntityCommands.h"

#include "Core/Assert.h"
#include "Core/CommandQueue.h"
#include "Core/Inventory.h"

#include "Core/Interfaces/IEngineComponent.h"
#include "Core/Interfaces/IEventListener.h"


namespace fr
{
	class BoundsComponent;


	class EntityManager final : public virtual en::IEngineComponent, public virtual core::IEventListener
	{
	public:
		static EntityManager* Get(); // Singleton functionality


	public: // IEngineComponent interface:
		void Startup() override;
		void Shutdown() override;

		void Update(uint64_t frameNum, double stepTimeMs) override;


	public:
		void SetInventory(core::Inventory*); // Dependency injection: Call once immediately after creation
		core::Inventory* GetInventory() const;
	private:
		core::Inventory* m_inventory;


	private: // Systems:
		void UpdateCameraController(double stepTimeMs);
		void UpdateAnimationControllers(double stepTimeMs);
		void UpdateAnimations(double stepTimeMs);
		void UpdateTransforms();
		void UpdateBounds();
		void UpdateMaterials();
		void UpdateLightsAndShadows();
		void UpdateCameras();

		void ExecuteDeferredDeletions();


	private:
		template<typename RenderDataType, typename CmptType, typename... OtherCmpts>
		void EnqueueRenderUpdateHelper();

	public: // Public interface:
		void EnqueueRenderUpdates();

	
		// Command queue interface:
	public:
		template<typename T, typename... Args>
		void EnqueueEntityCommand(Args&&... args);

		void EnqueueEntityCommand(std::function<void(void)>&&);


	private:
		void SetMainCamera(entt::entity);
		friend fr::SetMainCameraCommand;

		void SetActiveAmbientLight(entt::entity);
		friend fr::SetActiveAmbientLightCommand;

		entt::entity GetActiveAmbientLight() const;

		fr::BoundsComponent const* GetSceneBounds() const;
		entt::entity GetMainCamera() const;

		void Reset();
		
	private:
		void ProcessEntityCommands();
		core::CommandManager m_entityCommands;


	public:
		void ShowSceneObjectsImGuiWindow(bool* show);
		void ShowSceneTransformImGuiWindow(bool* show);
		void ShowImGuiEntityComponentDebug(bool* show);

	private:
		void ShowImGuiEntityComponentDebugHelper(
			std::vector<entt::entity> rootEntities, bool expandAll, bool expandChangeTriggered);
		void ShowImGuiEntityComponentDebugHelper(
			entt::entity rootEntity, bool expandAll, bool expandChangeTriggered);


	private: // IEventListener interface:
		void HandleEvents() override;


	private: // Configure event listeners etc
		void ConfigureRegistry();
		void OnBoundsDirty();

		void RegisterEntityForDelete(entt::entity);


	public: // EnTT wrappers:
		entt::entity CreateEntity(std::string const& name);
		entt::entity CreateEntity(char const* name);

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
		bool HasComponents(entt::entity entity) const;

		template<typename T, typename... Args>
		std::vector<entt::entity> GetAllEntities() const;

		template<typename T, typename... Args>
		bool EntityExists() const;


	private:
		entt::basic_registry<entt::entity> m_registry; // uint32_t entities
		mutable std::recursive_mutex m_registeryMutex;

		std::vector<entt::entity> m_deferredDeleteQueue;
		std::mutex m_deferredDeleteQueueMutex;


	private:
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
		EntityManager() = delete;
	public:
		EntityManager(PrivateCTORTag);
	};


	inline void EntityManager::SetInventory(core::Inventory* inventory)
	{
		m_inventory = inventory;
	}


	inline core::Inventory* EntityManager::GetInventory() const
	{
		return m_inventory;
	}


	template<typename T>
	void EntityManager::EmplaceComponent(entt::entity entity)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			m_registry.emplace<T>(entity);
		}
	}


	template<typename T, typename... Args>
	T* EntityManager::EmplaceComponent(entt::entity entity, Args&&... args)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			T& result = m_registry.emplace<T>(entity, std::forward<Args>(args)...);

			return &result;
		}
	}


	template<typename T>
	void EntityManager::EmplaceOrReplaceComponent(entt::entity entity)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			m_registry.emplace_or_replace<T>(entity);
		}
	}


	template<typename T>
	void EntityManager::TryEmplaceComponent(entt::entity entity)
	{
		if (!HasComponent<T>(entity))
		{
			EmplaceComponent<T>(entity);
		}
	}


	template<typename T, typename... Args>
	T* EntityManager::TryEmplaceComponent(entt::entity entity, Args &&...args)
	{
		T* existingComponent = TryGetComponent<T>(entity);
		if (existingComponent == nullptr)
		{
			existingComponent = EmplaceComponent<T>(entity, std::forward<Args>(args)...);
		}
		return existingComponent;
	}


	template<typename T, typename... Args>
	T* EntityManager::GetOrEmplaceComponent(entt::entity entity, Args&&... args)
	{
		T* existingComponent = TryGetComponent<T>(entity);
		if (existingComponent == nullptr)
		{
			existingComponent = EmplaceComponent<T, Args...>(entity, std::forward<Args>(args)...);
		}
		return existingComponent;
	}


	template<typename T>
	void EntityManager::RemoveComponent(entt::entity entity)
	{
		{
			// It's only safe to add/remove/iterate components if no other thread is adding/removing/iterating
			// components of the same type. For now, we obtain an exclusive lock on the entire registry, but this could
			// be more granular
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			m_registry.erase<T>(entity);
		}
	}


	template<typename T>
	T* EntityManager::TryGetComponent(entt::entity entity)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			return m_registry.try_get<T>(entity);
		}
	}


	template<typename T>
	T const* EntityManager::TryGetComponent(entt::entity entity) const
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			return m_registry.try_get<T>(entity);
		}
	}


	template<typename T>
	bool EntityManager::HasComponent(entt::entity entity) const
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
			return m_registry.any_of<T>(entity);
		}
	}


	template<typename T, typename... Args>
	bool EntityManager::HasComponents(entt::entity entity) const
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
			return m_registry.all_of<T, Args...>(entity);
		}
	}


	template<typename T>
	T& EntityManager::GetComponent(entt::entity entity)
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
			return m_registry.get<T>(entity);
		}
	}


	template<typename T>
	T const& EntityManager::GetComponent(entt::entity entity) const
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);
			return m_registry.get<T>(entity);
		}
	}


	template<typename T, typename... Args>
	std::vector<entt::entity> EntityManager::GetAllEntities() const
	{
		std::vector<entt::entity> result;

		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto view = m_registry.view<T, Args...>();
			for (auto entity : view)
			{
				result.emplace_back(entity);
			}
		}

		return result;
	}


	template<typename T, typename... Args>
	bool EntityManager::EntityExists() const
	{
		{
			std::unique_lock<std::recursive_mutex> lock(m_registeryMutex);

			auto view = m_registry.view<T, Args...>();
			for (auto entity : view)
			{
				return true; // If we hit this even once, an entity must exists with the given components
			}
		}
		return false;
	}



	template<typename T, typename... Args>
	void EntityManager::EnqueueEntityCommand(Args&&... args)
	{
		m_entityCommands.Enqueue<T>(std::forward<Args>(args)...);
	}


	inline void EntityManager::EnqueueEntityCommand(std::function<void(void)>&& lambdaCmd)
	{
		m_entityCommands.Enqueue(std::move(lambdaCmd));
	}
}