// © 2022 Adam Badke. All rights reserved.
#include "PerformanceTimer.h"
#include "Assert.h"


namespace util
{
	PerformanceTimer::PerformanceTimer() 
		: m_isStarted(false)
	{
	}


	PerformanceTimer::~PerformanceTimer()
	{
		SEAssert(!m_isStarted, "Timer started, but not stopped");
	}


	void PerformanceTimer::Start()
	{
		SEAssert(!m_isStarted, "Timer has already been started");
		m_isStarted = true;
		m_startTime = std::chrono::steady_clock::now();
	}


	double PerformanceTimer::PeekMs() const
	{
		SEAssert(m_isStarted, "Timer has not been started");
		const std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - m_startTime).count() / 1000000.f;
	}


	double PerformanceTimer::PeekSec() const
	{
		SEAssert(m_isStarted, "Timer has not been started");
		const std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
		return std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - m_startTime).count() / 1000000000.f;
	}


	double PerformanceTimer::StopMs()
	{
		const double msTime = PeekMs();
		m_isStarted = false;
		return msTime;
	} 


	double PerformanceTimer::StopSec()
	{
		const double secTime = PeekSec();
		m_isStarted = false;
		return secTime;
	}
}