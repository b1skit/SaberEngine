#pragma once

#include "SaberObject.h"


namespace SaberEngine
{
	// Forward declarations:
	class CoreEngine;


	// EngineComponent Interface: Functionality common to components in main game loop
	class EngineComponent : public SaberObject
	{
	public:
		EngineComponent(string name) : SaberObject(name) {}

		virtual void Startup() = 0; // We can't control the order constructors are called, so this is called to start the object
		virtual void Shutdown() = 0;
		virtual void Update() = 0;

	protected:


	private:

	};
}