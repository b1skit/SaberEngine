// © 2025 Adam Badke. All rights reserved.
#include "Private/PerformanceTimer.h"
#include "Private/PerformanceTimer_Win32.h"


namespace win32
{
	void PerformanceTimer::Create(host::PerformanceTimer& timer)
	{
		// Retrieve the frequency of the performance counter.
		// Note: The performance counter frequency is fixed at system boot and is consistent across all processors, thus
		// only needs to be queried once
		LARGE_INTEGER largeInt;
		QueryPerformanceFrequency(&largeInt); // Guaranteed to succeed on WindowsXP or later

		// Output is in counts per second (Hz), we convert it here to counts per ms
		timer.m_frequency = double(largeInt.QuadPart) / 1000.0;
	}


	void PerformanceTimer::Start(host::PerformanceTimer& timer)
	{
		LARGE_INTEGER largeInt;
		QueryPerformanceCounter(&largeInt); // Guaranteed to succeed on WindowsXP or later

		timer.m_startTime = largeInt.QuadPart;
	}


	double PerformanceTimer::PeekMs(host::PerformanceTimer const& timer)
	{
		LARGE_INTEGER largeInt;
		QueryPerformanceCounter(&largeInt); // Guaranteed to succeed on WindowsXP or later

		return static_cast<double>(largeInt.QuadPart - timer.m_startTime) / timer.m_frequency;
	}


	double PerformanceTimer::PeekSec(host::PerformanceTimer const& timer)
	{
		return PeekMs(timer) / 1000.0;
	}
}