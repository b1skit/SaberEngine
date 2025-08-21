// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace win32
{
	class PerformanceTimer;
}

namespace host
{
	class PerformanceTimer final
	{
	public:
		PerformanceTimer();
		~PerformanceTimer();

		void Start() noexcept;
		double PeekMs() const noexcept; // Gets the current delta (in ms) since Start(), without stopping
		double PeekSec() const noexcept; // Gets the current delta (in seconds) since Start(), without stopping

		double StopMs() noexcept; // Stops the timer, and returns the high precision time since Start() in ms
		double StopSec() noexcept; // Stops the timer, and returns the high precision time since Start() in seconds
		
		void Stop() noexcept; // Stops the timer, but does not return the time

		void Reset() noexcept; // Stops and restarts the timer

		bool IsRunning() const noexcept; // Has the timer been started?


	private:
		friend class win32::PerformanceTimer;

		uint64_t m_startTime;
		double m_frequency; // Counts per ms

		bool m_isStarted;
	};


	inline bool PerformanceTimer::IsRunning() const noexcept
	{
		return m_isStarted;
	}
}