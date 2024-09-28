// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace en
{
	// IEngineComponent Interface: Functionality common to components in main game loop
	class IEngineComponent
	{
	public: // IEngineComponent interface:
		virtual void Update(uint64_t frameNum, double stepTimeMs) = 0;

		virtual void Startup() = 0; // We can't control construction order, so this is called to start the object
		virtual void Shutdown() = 0;

	public:
		IEngineComponent() = default;
		~IEngineComponent() = default;
		IEngineComponent(IEngineComponent&&) noexcept = default;
		IEngineComponent& operator=(IEngineComponent&&) noexcept = default;

	private: // No copying allowed
		IEngineComponent(IEngineComponent const&) = delete;
		IEngineComponent& operator=(IEngineComponent const&) = delete;
	};
}