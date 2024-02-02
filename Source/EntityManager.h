// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"
#include "EngineComponent.h"
#include "EventListener.h"
#include "RelationshipComponent.h"


namespace fr
{
	class BoundsComponent;
	class PlayerObject;


	class EntityManager final : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public:
		static EntityManager* Get(); // Singleton functionality


	public: // EngineComponent interface:
		void Startup() override;
		void Shutdown() override;

		void Update(uint64_t frameNum, double stepTimeMs) override;


	private: // Systems:
		void UpdateCameraController(double stepTimeMs);
		void UpdateTransforms();
		void UpdateSceneBounds();
		void UpdateMaterials();
		void UpdateLightsAndShadows();
		void UpdateCameras();


	private:
		template<typename T, typename R>
		void EnqueueRenderUpdateHelper();

	public: // Public interface:
		void EnqueueRenderUpdates();

		fr::BoundsComponent const* GetSceneBounds() const;

		void SetMainCamera(entt::entity);
		entt::entity GetMainCamera() const;

		void ShowSceneObjectsImGuiWindow(bool* show);
		void ShowSceneTransformImGuiWindow(bool* show);
		void ShowImGuiEntityComponentDebug(bool* show) const;


	private:
		void SetMainCameraInternal(entt::entity);
		entt::entity GetMainCameraInternal() const;

	private: // EventListener interface:
		void HandleEvents() override;


	private: // Configure event listeners etc
		void ConfigureRegistry();

		void OnBoundsDirty();


	public: // EnTT wrappers:
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


		// Relationships:
	public:
		template<typename T>
		bool IsInHierarchyAbove(entt::entity); // Searches current entity and above

		template<typename T>
		T* GetFirstInHierarchyAbove(entt::entity); // Searches current entity and above

		template<typename T>
		T* GetFirstAndEntityInHierarchyAbove(entt::entity, entt::entity& owningEntityOut); // Searches current entity and above

		template<typename T>
		T* GetFirstAndEntityInChildren(entt::entity, entt::entity& childEntityOut); // Searches direct descendent children only (depth 1)

		template<typename T>
		T* GetFirstInChildren(entt::entity);


	private: // Private, lockless versions of the Relationship functions. Convenience for when a lock is already held
		template<typename T>
		bool IsInHierarchyAboveInternal(entt::entity);

		template<typename T>
		T* GetFirstInHierarchyAboveInternal(entt::entity);

		template<typename T>
		T* GetFirstAndEntityInHierarchyAboveInternal(entt::entity, entt::entity& owningEntityOut);

		template<typename T>
		T* GetFirstAndEntityInChildrenInternal(entt::entity, entt::entity& childEntityOut);

		template<typename T>
		T* GetFirstInChildrenInternal(entt::entity);


	private:
		entt::basic_registry<entt::entity> m_registry; // uint32_t entities
		mutable std::shared_mutex m_registeryMutex;

		bool m_processInput;


	private:
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
		EntityManager() = delete;
	public:
		EntityManager(PrivateCTORTag);
	};


	template<typename T>
	entt::entity EntityManager::GetEntityFromComponent(T const& component) const
	{
		entt::entity entity = entt::null;
		{
			std::shared_lock<std::shared_mutex> lock(m_registeryMutex);

			entity = entt::to_entity(m_registry, component);
		}
		SEAssert(entity != entt::null, "Entity not found");

		return entity;
	}


	template<typename T>
	void EntityManager::EmplaceComponent(entt::entity entity)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			m_registry.emplace<T>(entity);
		}
	}


	template<typename T, typename... Args>
	T* EntityManager::EmplaceComponent(entt::entity entity, Args&&... args)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			T& result = m_registry.emplace<T>(entity, std::forward<Args>(args)...);

			return &result;
		}
	}


	template<typename T>
	void EntityManager::EmplaceOrReplaceComponent(entt::entity entity)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

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
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			m_registry.erase<T>(entity);
		}
	}


	template<typename T>
	T* EntityManager::TryGetComponent(entt::entity entity)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			return m_registry.try_get<T>(entity);
		}
	}


	template<typename T>
	T const* EntityManager::TryGetComponent(entt::entity entity) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			return m_registry.try_get<T>(entity);
		}
	}


	template<typename T>
	bool EntityManager::HasComponent(entt::entity entity) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);
			
			return m_registry.any_of<T>(entity);
		}
	}


	template<typename T>
	T& EntityManager::GetComponent(entt::entity entity)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);
			return m_registry.get<T>(entity);
		}
	}


	template<typename T>
	T const& EntityManager::GetComponent(entt::entity entity) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);
			return m_registry.get<T>(entity);
		}
	}


	// --- 
	// Relationships
	// ---


	template<typename T>
	bool EntityManager::IsInHierarchyAbove(entt::entity entity)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			return IsInHierarchyAboveInternal<T>(entity);
		}
	}


	template<typename T>
	T* EntityManager::GetFirstInHierarchyAbove(entt::entity entity)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			return GetFirstInHierarchyAboveInternal<T>(entity);
		}
	}


	template<typename T>
	T* EntityManager::GetFirstAndEntityInHierarchyAbove(entt::entity entity, entt::entity& owningEntityOut)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			return GetFirstAndEntityInHierarchyAboveInternal<T>(entity, owningEntityOut);
		}
	}


	template<typename T>
	T* EntityManager::GetFirstAndEntityInChildren(entt::entity entity, entt::entity& childEntityOut)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			return GetFirstAndEntityInChildrenInternal<T>(entity, childEntityOut);
		}
	}


	template<typename T>
	T* EntityManager::GetFirstInChildren(entt::entity entity)
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_registeryMutex);

			return GetFirstInChildrenInternal<T>(entity);
		}
	}


	// ---
	// Lockless internal Relationship functions
	// ---


	template<typename T>
	bool EntityManager::IsInHierarchyAboveInternal(entt::entity entity)
	{
		return GetFirstInHierarchyAboveInternal<T>(entity) != nullptr;
	}


	template<typename T>
	T* EntityManager::GetFirstInHierarchyAboveInternal(entt::entity entity)
	{
		entt::entity dummy = entt::null;
		return GetFirstAndEntityInHierarchyAboveInternal<T>(entity, dummy);
	}


	template<typename T>
	T* EntityManager::GetFirstAndEntityInHierarchyAboveInternal(entt::entity entity, entt::entity& owningEntityOut)
	{
		SEAssert(entity != entt::null, "Entity cannot be null");

		entt::entity currentEntity = entity;
		while (currentEntity != entt::null)
		{
			T* component = m_registry.try_get<T>(currentEntity);
			if (component != nullptr)
			{
				owningEntityOut = currentEntity;
				return component;
			}

			fr::Relationship const& currentRelationship = m_registry.get<fr::Relationship>(currentEntity);
			
			currentEntity = currentRelationship.GetParent();
		}

		return nullptr;
	}


	template<typename T>
	T* EntityManager::GetFirstAndEntityInChildrenInternal(entt::entity entity, entt::entity& childEntityOut)
	{
		SEAssert(entity != entt::null, "Invalid entity");

		childEntityOut = entt::null;

		fr::Relationship const& entityRelationship = m_registry.get<fr::Relationship>(entity);
		const entt::entity firstChild = entityRelationship.GetFirstChild();
		entt::entity current = firstChild;
		do
		{
			fr::Relationship const& currentRelationship = m_registry.get<fr::Relationship>(current);

			T* component = m_registry.try_get<T>(current);
			if (component)
			{
				childEntityOut = current;
				return component;
			}

			current = currentRelationship.GetNext();
		} while (current != firstChild);

		return nullptr;
	}


	template<typename T>
	T* EntityManager::GetFirstInChildrenInternal(entt::entity entity)
	{
		entt::entity dummy = entt::null;
		return GetFirstAndEntityInChildrenInternal<T>(entity, dummy);
	}
}