#pragma once
#include "EventManager.h"
#include "LogManager.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "enEngineConfig.h"


namespace SaberEngine
{

	// CORE ENGINE:
	class CoreEngine : public SaberObject, public EventListener
	{
	public:
		CoreEngine(int argc, char** argv);

		// Static Engine component singletons getters:		
		static inline CoreEngine*		GetCoreEngine()		{ return coreEngine; }
		static inline EventManager*		GetEventManager()	{ return SaberEventManager; }
		static inline InputManager*		GetInputManager()	{ return SaberInputManager; }
		static inline SceneManager*		GetSceneManager()	{ return SaberSceneManager; }
		static inline RenderManager*	GetRenderManager()	{ return SaberRenderManager; }
		
		// Lifetime flow:
		void Startup();
		void Run();
		void Stop();
		void Shutdown();

		// Member functions
		static en::EngineConfig const* GetConfig();

		// SaberObject interface:
		void Update();

		// EventListener interface:
		void HandleEvent(EventInfo const* eventInfo);

		
	private:	
		// Constants:
		const double FIXED_TIMESTEP = 1000.0 / 120.0; // Regular step size, in ms
		//const double MAX_TIMESTEP = 0.5;	// Max amount of time before giving up

		// Private engine component singletons:	
		LogManager* const	SaberLogManager		= &LogManager::Instance();
		TimeManager* const	SaberTimeManager	= &TimeManager::Instance();

		static en::EngineConfig m_config;

		// Static Engine component singletons
		static CoreEngine*		coreEngine;
		static EventManager*	SaberEventManager;
		static InputManager*	SaberInputManager;
		static SceneManager*	SaberSceneManager;
		static RenderManager*	SaberRenderManager;
		

		// Engine control:
		bool m_isRunning = false;


		bool ProcessCommandLineArgs(int argc, char** argv);
	};
}
