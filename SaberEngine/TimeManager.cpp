#include <SDL.h>

#include "TimeManager.h"
#include "DebugConfiguration.h"


namespace en
{
	// Static values:
	uint64_t TimeManager::m_prevTimeMs;
	uint64_t TimeManager::m_currentTimeMs;
	uint64_t TimeManager::m_deltaTimeMs;


	TimeManager::TimeManager()
	{
		const uint64_t numTicks = SDL_GetTicks64();
		m_prevTimeMs = numTicks;
		m_currentTimeMs = numTicks;
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
		m_prevTimeMs	= m_currentTimeMs;
		m_currentTimeMs = SDL_GetTicks64();
		m_deltaTimeMs	= (m_currentTimeMs - m_prevTimeMs);
	}
}

