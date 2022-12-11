#pragma once

#include <memory>
#include <vector>
#include "EngineComponent.h"
#include "Updateable.h"


namespace fr
{
	class GameplayManager : public virtual en::EngineComponent
	{
	public:
		static GameplayManager* Get(); // Singleton functionality

	public:
		virtual void Update(const double stepTimeMs) override;

		virtual void Startup();
		virtual void Shutdown();


	private:
		std::vector<std::shared_ptr<en::Updateable>> m_updateables;
	};
}