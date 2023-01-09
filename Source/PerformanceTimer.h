// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace util
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

	private:
		std::chrono::steady_clock::time_point m_startTime;
		bool m_isStarted;
	};
}