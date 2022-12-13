#pragma once


namespace en
{
	// EngineComponent Interface: Functionality common to components in main game loop
	class EngineComponent
	{
	public:
		EngineComponent() = default;
		EngineComponent(EngineComponent const&) = default;
		EngineComponent(EngineComponent&&) = default;
		EngineComponent& operator=(EngineComponent const&) = default;
		
		virtual ~EngineComponent() = 0;

		// Updateable interface:
		virtual void Update(uint64_t frameNum, double stepTimeMs) = 0;

		// EngineComponent interface:
		virtual void Startup() = 0; // We can't control construction order, so this is called to start the object
		virtual void Shutdown() = 0;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline EngineComponent::~EngineComponent() {};
}