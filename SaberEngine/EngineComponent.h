#pragma once

#include "Updateable.h"


namespace en
{
	// EngineComponent Interface: Functionality common to components in main game loop
	class EngineComponent : public en::Updateable
	{
	public:
		EngineComponent() = default;
		EngineComponent(EngineComponent const&) = default;
		EngineComponent(EngineComponent&&) = default;
		EngineComponent& operator=(EngineComponent const&) = default;
		~EngineComponent() = default;

		// Updateable interface:
		virtual void Update() override = 0;

		// EngineComponent interface:
		virtual void Startup() = 0; // We can't control construction order, so this is called to start the object
		virtual void Shutdown() = 0;
	};
}