// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace win32
{
	class PerformanceTimer;
}

namespace host
{
	class PerformanceTimer
	{
	public:
		PerformanceTimer();
		~PerformanceTimer();

		void Start();
		double PeekMs() const; // Gets the current delta (in ms) since Start(), without stopping
		double PeekSec() const; // Gets the current delta (in seconds) since Start(), without stopping

		double StopMs(); // Stops the timer, and returns the high precision time since Start() in ms
		double StopSec(); // Stops the timer, and returns the high precision time since Start() in seconds

		bool IsRunning() const; // Has the timer been started?


	private:
		friend class win32::PerformanceTimer;

		uint64_t m_startTime;
		double m_frequency; // Counts per ms

		bool m_isStarted;
	};


	inline bool PerformanceTimer::IsRunning() const
	{
		return m_isStarted;
	}
}