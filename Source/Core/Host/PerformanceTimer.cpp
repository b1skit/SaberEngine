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


	void PerformanceTimer::Start() noexcept
	{
		SEAssert(!m_isStarted, "Timer has already been started");
		m_isStarted = true;

		platform::PerformanceTimer::Start(*this);
	}


	double PerformanceTimer::PeekMs() const noexcept
	{
		if (m_isStarted)
		{
			return platform::PerformanceTimer::PeekMs(*this);
		}
		return 0.0;
	}


	double PerformanceTimer::PeekSec() const noexcept
	{
		if (m_isStarted)
		{
			return platform::PerformanceTimer::PeekSec(*this);
		}
		return 0.0;
	}


	double PerformanceTimer::StopMs() noexcept
	{
		const double msTime = PeekMs();
		m_isStarted = false;
		return msTime;
	} 


	double PerformanceTimer::StopSec() noexcept
	{
		const double secTime = PeekSec();
		m_isStarted = false;
		return secTime;
	}


	void PerformanceTimer::Stop() noexcept
	{
		m_isStarted = false;
	}


	void PerformanceTimer::Reset() noexcept
	{
		Stop();
		Start();
	}
}