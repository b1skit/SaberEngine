#pragma once

#include <memory>

#include "EventManager.h"
#include "LogManager.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "EngineConfig.h"


namespace en
{
	class CoreEngine : public virtual SaberEngine::SaberObject, public virtual SaberEngine::EventListener
	{
	public:
		CoreEngine(int argc, char** argv);
		~CoreEngine() = default;
		
		CoreEngine() = delete;
		CoreEngine(CoreEngine const&) = delete;
		CoreEngine(CoreEngine&&) = delete;
		CoreEngine& operator=(CoreEngine const&) = delete;

		// Static Engine component singletons getters:		
		static inline CoreEngine*					GetCoreEngine()		{ return m_coreEngine; }
		static inline SaberEngine::EventManager*	GetEventManager()	{ return m_eventManager.get(); }
		static inline SaberEngine::InputManager*	GetInputManager()	{ return m_inputManager.get(); }
		static inline SaberEngine::SceneManager*	GetSceneManager()	{ return m_sceneManager.get(); }
		static inline re::RenderManager*			GetRenderManager()	{ return m_renderManager.get(); }
		
		// Lifetime flow:
		void Startup();
		void Run();
		void Stop();
		void Shutdown();

		// Member functions
		static en::EngineConfig const* GetConfig();

		// SaberObject interface:
		void Update() override;

		// EventListener interface:
		void HandleEvent(std::shared_ptr<SaberEngine::EventInfo const> eventInfo) override;

		
	private:	
		// Constants:
		const double m_FixedTimeStep; // Regular step size, in ms
		//const double MAX_TIMESTEP = 0.5;	// Max amount of time before giving up

		bool m_isRunning;

		static en::EngineConfig m_config;

		// Private engine component singletons:	
		std::shared_ptr<fr::LogManager> const	m_logManager;
		std::shared_ptr<SaberEngine::TimeManager> const		m_timeManager;

		// Static Engine component singletons
		static CoreEngine*									m_coreEngine;
		static std::shared_ptr<SaberEngine::EventManager>	m_eventManager;
		static std::shared_ptr<SaberEngine::InputManager>	m_inputManager;
		static std::shared_ptr<SaberEngine::SceneManager>	m_sceneManager;
		static std::shared_ptr<re::RenderManager>			m_renderManager;


		bool ProcessCommandLineArgs(int argc, char** argv);
	};
}
