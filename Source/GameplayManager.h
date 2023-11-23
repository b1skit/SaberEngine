// © 2022 Adam Badke. All rights reserved.
#pragma once
#include <entt/entity/registry.hpp>

#include "EngineComponent.h"
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
}