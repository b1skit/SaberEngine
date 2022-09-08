#pragma once

#include "SaberObject.h"


namespace SaberEngine
{
	// EngineComponent Interface: Functionality common to components in main game loop
	class EngineComponent : public SaberObject
	{
	public:
		EngineComponent(std::string const& name) : SaberObject(name) {}

		virtual ~EngineComponent() = 0;

		virtual void Startup() = 0; // We can't control construction order, so this is called to start the object
		virtual void Shutdown() = 0;
		virtual void Update() = 0;

		EngineComponent() = delete;
	};

	// We need to provide a destructor implementation since it's pure virtual
	inline EngineComponent::~EngineComponent() {}
}