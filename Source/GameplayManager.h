// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <entt/entity/registry.hpp>

#include "EngineComponent.h"
#include "NamedObject.h"
#include "Updateable.h"


namespace fr
{
	class PlayerObject;
	class Transformable;


	class GameplayManager final : public virtual en::EngineComponent
	{
	public:
		static GameplayManager* Get(); // Singleton functionality


	public:
		void Startup() override;
		void Shutdown() override;

		void Update(uint64_t frameNum, double stepTimeMs) override;


		// EnTT wrappers:
		entt::entity CreateEntity();

		template<typename T>
		entt::entity GetEntityFromComponent(T const&) const;

		template<typename T, typename... Args>
		T* EmplaceComponent(entt::entity, Args &&...args);


	private:
		entt::basic_registry<entt::entity> m_registry; // uint32_t entities
		std::shared_mutex m_registeryMutex;



		// DEPRECATED:
	protected:
		friend class en::Updateable;
		void AddUpdateable(en::Updateable* updateable);
		void RemoveUpdateable(en::Updateable const* updateable);


	protected:
		friend class fr::Transformable;
		void AddTransformable(fr::Transformable*);
		void RemoveTransformable(fr::Transformable const*);


	private:
		void UpdateUpdateables(double stepTimeMs) const;
		void UpdateTransformables() const;


	private:
		std::vector<en::Updateable*> m_updateables;
		mutable std::mutex m_updateablesMutex;

		std::vector<fr::Transformable*> m_transformables;
		mutable std::mutex m_transformablesMutex;

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


	template<typename T, typename... Args>
	T* GameplayManager::EmplaceComponent(entt::entity entity, Args &&...args)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_registeryMutex);

			T& result = m_registry.emplace<T>(entity, std::forward<Args>(args)...);

			return &result;
		}
	}
}