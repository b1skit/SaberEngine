// © 2022 Adam Badke. All rights reserved.
#include "PerformanceTimer.h"
#include "PerformanceTimer_Platform.h"

#include "../Assert.h"


namespace host
{
	PerformanceTimer::PerformanceTimer() 
		: m_startTime(0)
		, m_frequency(0.0)
		, m_isStarted(false)
	{
		platform::PerformanceTimer::Create(*this);
	}


	PerformanceTimer::~PerformanceTimer()
	{
		SEAssert(!m_isStarted, "Timer started, but not stopped");
	}


	void PerformanceTimer::Start()
	{
		SEAssert(!m_isStarted, "Timer has already been started");
		m_isStarted = true;

		platform::PerformanceTimer::Start(*this);
	}


	double PerformanceTimer::PeekMs() const
	{
		SEAssert(m_isStarted, "Timer has not been started");

		return platform::PerformanceTimer::PeekMs(*this);
	}


	double PerformanceTimer::PeekSec() const
	{
		SEAssert(m_isStarted, "Timer has not been started");

		return platform::PerformanceTimer::PeekSec(*this);
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