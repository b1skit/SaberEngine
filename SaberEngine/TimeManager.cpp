#include <SDL.h>

#include "TimeManager.h"
#include "DebugConfiguration.h"


namespace en
{
	// Static values:
	unsigned int TimeManager::m_startTime;
	unsigned int TimeManager::m_prevTime;
	unsigned int TimeManager::m_currentTime;
	unsigned int TimeManager::m_deltaTime;


	TimeManager::TimeManager()
	{
		m_startTime = m_prevTime = m_currentTime = SDL_GetTicks();
	}

	void TimeManager::Startup()
	{
		LOG("TimeManager starting...");
	}


	void TimeManager::Shutdown()
	{
		LOG("Time manager shutting down...");
	}


	void TimeManager::Update()
	{
		m_prevTime	= m_currentTime;
		m_currentTime = SDL_GetTicks();
		m_deltaTime	= (m_currentTime - m_prevTime);
	}
}

