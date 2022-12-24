#pragma once

#include "EngineComponent.h"
#include "Updateable.h"


namespace fr
{
	class GameplayManager final : public virtual en::EngineComponent
	{
	public:
		static GameplayManager* Get(); // Singleton functionality

	public:
		virtual void Update(uint64_t frameNum, double stepTimeMs) override;

		virtual void Startup();
		virtual void Shutdown();


	private:
		std::vector<std::shared_ptr<en::Updateable>> m_updateables;
	};
}