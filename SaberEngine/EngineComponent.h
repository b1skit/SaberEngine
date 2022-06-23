// Interface for Saber engine components
// Inherits from SaberObject

#pragma once
#include "SaberObject.h"	// Base class


namespace SaberEngine
{
	// Forward declarations:
	class CoreEngine;

	// EngineComponent Interface: Functionality common to components in main game loop
	class EngineComponent : public SaberObject
	{
	public:
		EngineComponent(string name) : SaberObject(name)
		{
		}

		// We can't control the order constructors are called, so this function should be called to start the object
		virtual void Startup() = 0;
		
		virtual void Shutdown() = 0;

		/*virtual void Update() = 0;*/


	protected:


	private:


	};
}