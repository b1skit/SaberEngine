// © 2022 Adam Badke. All rights reserved.
#include "PerformanceTimer.h"
#include "DebugConfiguration.h"


namespace util
{
	PerformanceTimer::PerformanceTimer() 
		: m_startTime(0)
		, m_isStarted(false)
	{
	}


	PerformanceTimer::~PerformanceTimer()
	{
		SEAssert("Timer started, but not stopped", !m_isStarted);
	}


	void PerformanceTimer::Start()
	{
		SEAssert("Timer has already been started", !m_isStarted);
		m_isStarted = true;
		m_startTime = SDL_GetPerformanceCounter();
	}


	double PerformanceTimer::PeekMs()
	{
		SEAssert("Timer has not been started", m_isStarted);
		const uint64_t currentTime = SDL_GetPerformanceCounter();
		return ((currentTime - m_startTime) * 1000.0) / SDL_GetPerformanceFrequency();
	}


	double PerformanceTimer::PeekSec()
	{
		constexpr double oneOverOneThousand = 1.0 / 1000.0;
		return PeekMs() * oneOverOneThousand;
	}


	double PerformanceTimer::StopMs()
	{
		m_isStarted = false;
		const uint64_t currentTime = SDL_GetPerformanceCounter();
		return ((currentTime - m_startTime) * 1000.0) / SDL_GetPerformanceFrequency();
	} 


	double PerformanceTimer::StopSec()
	{
		constexpr double oneOverOneThousand = 1.0 / 1000.0;
		return StopMs() * oneOverOneThousand;
	}
}