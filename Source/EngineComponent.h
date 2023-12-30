// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace en
{
	// EngineComponent Interface: Functionality common to components in main game loop
	class EngineComponent
	{
	public: // EngineComponent interface:
		virtual void Update(uint64_t frameNum, double stepTimeMs) = 0;

		virtual void Startup() = 0; // We can't control construction order, so this is called to start the object
		virtual void Shutdown() = 0;

	public:
		EngineComponent() = default;
		EngineComponent& operator=(EngineComponent const&) = default;
		EngineComponent& operator=(EngineComponent&&) = default;

	private: // No copying allowed
		EngineComponent(EngineComponent const&) = delete;
		EngineComponent(EngineComponent&&) = delete;
	};
}