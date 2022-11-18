#pragma once

#include <SDL.h>


namespace util
{
	class PerformanceTimer
	{
	public:
		PerformanceTimer();
		~PerformanceTimer();

		void Start();
		double PeekMs(); // Gets the current delta (in ms) since Start(), without stopping
		double PeekSec(); // Gets the current delta (in seconds) since Start(), without stopping

		double StopMs(); // Stops the timer, and returns the high precision time since Start() in ms
		double StopSec(); // Stops the timer, and returns the high precision time since Start() in seconds

	private:
		uint64_t m_startTime;
		bool m_isStarted;
	};
}